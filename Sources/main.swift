import AppKit
import ApplicationServices
import CoreGraphics

// copy4paste: un portapapeles ligero. macOS 10.15+, Intel.
private enum Config {
    static let capacityKey = "copy4paste.capacity"
    static let minimum = 1
    static let maximum = 30
    static var capacity: Int {
        let raw = UserDefaults.standard.object(forKey: capacityKey) as? Int ?? 4
        return min(max(raw, minimum), maximum)
    }
    static func setCapacity(_ n: Int) {
        UserDefaults.standard.set(min(max(n, minimum), maximum), forKey: capacityKey)
    }
}

// Un fondo con geometría explícita. No depende de vistas desplazables.
private class FlippedView: NSView {
    override var isFlipped: Bool { true }
}

private final class DeleteControl: NSView {
    var onDelete: (() -> Void)?
    override var isFlipped: Bool { true }
    override func draw(_ dirtyRect: NSRect) {
        super.draw(dirtyRect)
        let x = bounds.midX, y = bounds.midY
        let cross = NSBezierPath()
        cross.lineWidth = 1.7
        cross.lineCapStyle = .round
        cross.move(to: NSPoint(x: x - 4.3, y: y - 4.3))
        cross.line(to: NSPoint(x: x + 4.3, y: y + 4.3))
        cross.move(to: NSPoint(x: x - 4.3, y: y + 4.3))
        cross.line(to: NSPoint(x: x + 4.3, y: y - 4.3))
        NSColor(calibratedWhite: 0.86, alpha: 1).setStroke()
        cross.stroke()
    }
    override func mouseDown(with event: NSEvent) { onDelete?() }
}

private final class ClipboardRow: FlippedView {
    let index: Int
    var onSelect: ((Int) -> Void)?
    var onPaste: ((Int) -> Void)?
    var onDelete: ((Int) -> Void)?
    var isSelected = false { didSet { needsDisplay = true } }
    private var trackingArea: NSTrackingArea?
    private var deleteControl: DeleteControl?

    init(index: Int, text: String, width: CGFloat, height: CGFloat) {
        self.index = index
        super.init(frame: NSRect(x: 0, y: 0, width: width, height: height))
        let number = NSTextField(labelWithString: "\(index + 1)")
        number.font = NSFont.monospacedDigitSystemFont(ofSize: 11, weight: index == 0 ? .semibold : .regular)
        number.textColor = NSColor(calibratedWhite: 0.79, alpha: 1)
        number.frame = NSRect(x: 9, y: 7, width: 18, height: 18)
        addSubview(number)
        let preview = text.replacingOccurrences(of: "\n", with: " ↵ ")
                          .replacingOccurrences(of: "\r", with: " ")
                          .replacingOccurrences(of: "\t", with: " ⇥ ")
        let field = NSTextField(labelWithString: preview)
        field.font = NSFont.systemFont(ofSize: 12)
        field.textColor = .white
        field.lineBreakMode = .byTruncatingTail
        field.maximumNumberOfLines = 1
        field.frame = NSRect(x: 29, y: 6, width: width - 67, height: 20)
        addSubview(field)
        let delete = DeleteControl(frame: NSRect(x: width - 32, y: 4, width: 28, height: height - 8))
        delete.toolTip = "Borrar esta copia"
        delete.onDelete = { [weak self] in
            guard let self = self else { return }
            self.onDelete?(self.index)
        }
        addSubview(delete)
        deleteControl = delete
    }
    // Los labels son decorativos: siempre enviamos el clic a la fila o a la cruz.
    // Evita que NSTextField consuma los eventos del ratón.
    override func hitTest(_ point: NSPoint) -> NSView? {
        let localPoint = convert(point, from: superview)
        guard bounds.contains(localPoint) else { return nil }
        if let delete = deleteControl, delete.frame.contains(localPoint) { return delete }
        return self
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) no se utiliza") }
    override func updateTrackingAreas() {
        super.updateTrackingAreas()
        if let area = trackingArea { removeTrackingArea(area) }
        let area = NSTrackingArea(rect: .zero,
                                  options: [.mouseMoved, .mouseEnteredAndExited, .activeAlways, .inVisibleRect],
                                  owner: self, userInfo: nil)
        addTrackingArea(area)
        trackingArea = area
    }
    override func mouseEntered(with event: NSEvent) { onSelect?(index) }
    override func mouseMoved(with event: NSEvent) { onSelect?(index) }
    override func mouseDown(with event: NSEvent) { onPaste?(index) }
    override func draw(_ dirtyRect: NSRect) {
        super.draw(dirtyRect)
        if isSelected {
            NSColor(calibratedWhite: 1, alpha: 0.18).setFill()
            NSBezierPath(roundedRect: bounds.insetBy(dx: 1, dy: 1), xRadius: 5, yRadius: 5).fill()
        }
    }
}

private final class PopupPanel: NSPanel {
    var onMove: ((Int) -> Void)?
    var onPaste: (() -> Void)?
    var onDelete: (() -> Void)?
    var onEscape: (() -> Void)?
    var onDigit: ((Int) -> Void)?
    var onScroll: ((Int) -> Void)?
    override func scrollWheel(with event: NSEvent) {
        if abs(event.scrollingDeltaY) > 0.25 {
            onScroll?(event.scrollingDeltaY < 0 ? 1 : -1)
        } else { super.scrollWheel(with: event) }
    }
    override var canBecomeKey: Bool { true }
    override var canBecomeMain: Bool { false }
    override func keyDown(with event: NSEvent) {
        switch event.keyCode {
        case 53: onEscape?()                 // Esc
        case 125: onMove?(1)                 // Abajo
        case 126: onMove?(-1)                // Arriba
        case 36, 76: onPaste?()              // Intro
        case 51, 117: onDelete?()            // Borrar
        default:
            if let string = event.charactersIgnoringModifiers,
               let number = Int(string), (1...9).contains(number) {
                onDigit?(number - 1)
            } else { super.keyDown(with: event) }
        }
    }
}

private final class ClipboardMemory {
    private let clipboard = NSPasteboard.general
    private var observed = -1
    private var store = HistoryStore(capacity: Config.capacity)
    private(set) var isText = false
    var entries: [String] { store.entries }
    var capacity: Int { store.capacity }
    func changeCapacity(_ value: Int) { store.setCapacity(value) }

    func refresh() {
        let change = clipboard.changeCount
        guard change != observed else { return }
        observed = change
        let types = clipboard.types ?? []
        // Imágenes, archivos y entradas privadas jamás se añaden al historial.
        let ignored = ["org.nspasteboard.ConcealedType", "org.nspasteboard.TransientType", "org.nspasteboard.AutoGeneratedType"]
        if types.contains(.fileURL) || types.contains(.tiff) || types.contains(.png) ||
           ignored.contains(where: { types.contains(NSPasteboard.PasteboardType($0)) }) {
            isText = false
            return
        }
        guard let string = clipboard.string(forType: .string), !string.isEmpty else {
            isText = false
            return
        }
        isText = store.remember(string)
    }

    func remove(at index: Int) {
        refresh()
        guard entries.indices.contains(index) else { return }
        let old = entries[index]
        _ = store.remove(old)
        if clipboard.string(forType: .string) == old {
            clipboard.clearContents()
            if let first = entries.first { clipboard.setString(first, forType: .string) }
            observed = clipboard.changeCount
            isText = !entries.isEmpty
        }
    }
    func select(at index: Int) -> Bool {
        guard entries.indices.contains(index) else { return false }
        let selected = entries[index]
        clipboard.clearContents()
        clipboard.setString(selected, forType: .string)
        observed = clipboard.changeCount
        isText = true
        // No reordenamos al pegar: el orden muestra las últimas copias reales.
        return true
    }
}

private let syntheticPasteMarker: Int64 = 0xC0F4A57E

private final class Copy4PasteApp: NSObject, NSApplicationDelegate {
    private let clipboard = ClipboardMemory()
    private var popup: PopupPanel?
    private var rows: [ClipboardRow] = []
    private var displayed: [String] = []
    private var selected = 0
    private var firstRow = 0
    private var target: NSRunningApplication?
    private var keyboardTap: CFMachPort?
    private var pollTimer: Timer?
    private var retryTimer: Timer?
    private var dismissTimer: Timer?
    private var showingSettings = false
    private let width: CGFloat = 290
    private let rowHeight: CGFloat = 31

    func applicationDidFinishLaunching(_ notification: Notification) {
        // Sustituir limpiamente prototipos anteriores del mismo producto.
        // Evita la doble interceptación de ⌘V que afectaba a las versiones beta.
        let currentPID = ProcessInfo.processInfo.processIdentifier
        let oldBundleIDs: Set<String> = [
            "com.kidinoise.copy4paste",
            "com.kidinoise.copy4paste.cleantrial"
        ]
        for running in NSWorkspace.shared.runningApplications where running.processIdentifier != currentPID {
            if let bundleID = running.bundleIdentifier, oldBundleIDs.contains(bundleID) {
                _ = running.terminate()
            }
        }
        NSApp.setActivationPolicy(.accessory)
        clipboard.refresh()
        pollTimer = Timer.scheduledTimer(withTimeInterval: 0.25, repeats: true) { [weak self] _ in
            self?.clipboard.refresh()
        }
        retryTimer = Timer.scheduledTimer(withTimeInterval: 2, repeats: true) { [weak self] _ in
            self?.startTap()
        }
        let prompt = kAXTrustedCheckOptionPrompt.takeUnretainedValue() as String
        _ = AXIsProcessTrustedWithOptions([prompt: true] as CFDictionary)
        startTap()
    }

    private func startTap() {
        guard keyboardTap == nil, AXIsProcessTrusted() else { return }
        let mask = CGEventMask(1) << CGEventType.keyDown.rawValue
        guard let tap = CGEvent.tapCreate(tap: .cgSessionEventTap,
                                          place: .headInsertEventTap,
                                          options: .defaultTap,
                                          eventsOfInterest: mask,
                                          callback: eventTapCallback,
                                          userInfo: Unmanaged.passUnretained(self).toOpaque()) else { return }
        keyboardTap = tap
        let source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0)
        CFRunLoopAddSource(CFRunLoopGetMain(), source, .commonModes)
        CGEvent.tapEnable(tap: tap, enable: true)
        retryTimer?.invalidate()
    }

    func handle(type: CGEventType, event: CGEvent) -> Unmanaged<CGEvent>? {
        if type == .tapDisabledByTimeout || type == .tapDisabledByUserInput {
            if let tap = keyboardTap { CGEvent.tapEnable(tap: tap, enable: true) }
            return Unmanaged.passUnretained(event)
        }
        guard type == .keyDown else { return Unmanaged.passUnretained(event) }
        if event.getIntegerValueField(.eventSourceUserData) == syntheticPasteMarker {
            return Unmanaged.passUnretained(event)
        }
        let flags = event.flags
        let code = event.getIntegerValueField(.keyboardEventKeycode)
        if flags.contains(.maskCommand), flags.contains(.maskAlternate), flags.contains(.maskControl),
           !flags.contains(.maskShift), code == 35 { // Ctrl+Opt+Cmd+P
            DispatchQueue.main.async { [weak self] in self?.openSettings() }
            return nil
        }
        guard flags.contains(.maskCommand), !flags.contains(.maskShift),
              !flags.contains(.maskAlternate), !flags.contains(.maskControl), code == 9 else {
            return Unmanaged.passUnretained(event)
        }
        clipboard.refresh()
        guard clipboard.isText, clipboard.entries.count > 1 else {
            return Unmanaged.passUnretained(event)
        }
        if popup?.isVisible == true { return nil }
        target = NSWorkspace.shared.frontmostApplication
        DispatchQueue.main.async { [weak self] in self?.openSelector() }
        return nil
    }

    private func makePopup(height: CGFloat) -> PopupPanel {
        if let old = popup { old.orderOut(nil) }
        let panel = PopupPanel(contentRect: NSRect(x: 0, y: 0, width: width, height: height),
                               styleMask: [.borderless, .nonactivatingPanel], backing: .buffered, defer: false)
        panel.level = .floating
        panel.collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary]
        panel.isOpaque = false
        panel.backgroundColor = .clear
        panel.hasShadow = true
        panel.acceptsMouseMovedEvents = true
        panel.onMove = { [weak self] delta in self?.move(delta) }
        panel.onPaste = { [weak self] in self?.choose(self?.selected ?? 0) }
        panel.onDelete = { [weak self] in self?.delete(self?.selected ?? 0) }
        panel.onEscape = { [weak self] in self?.hide() }
        panel.onDigit = { [weak self] index in self?.choose(index) }
        panel.onScroll = { [weak self] delta in self?.scroll(delta) }
        popup = panel
        return panel
    }

    private func baseView(height: CGFloat) -> FlippedView {
        let view = FlippedView(frame: NSRect(x: 0, y: 0, width: width, height: height))
        view.wantsLayer = true
        view.layer?.cornerRadius = 9
        view.layer?.masksToBounds = true
        view.layer?.backgroundColor = NSColor(calibratedWhite: 0.15, alpha: 0.97).cgColor
        return view
    }

    private func present(_ panel: PopupPanel, height: CGFloat) {
        let cursor = NSEvent.mouseLocation
        let screen = NSScreen.screens.first(where: { $0.frame.contains(cursor) }) ?? NSScreen.main
        let frame = screen?.visibleFrame ?? NSRect(x: 0, y: 0, width: 1024, height: 768)
        let x = min(max(cursor.x + 9, frame.minX + 6), frame.maxX - width - 6)
        let y = min(max(cursor.y - height + 8, frame.minY + 6), frame.maxY - height - 6)
        panel.setFrame(NSRect(x: x, y: y, width: width, height: height), display: true)
        panel.makeKeyAndOrderFront(nil)
    }

    private func openSelector() {
        guard popup?.isVisible != true else { return }
        clipboard.refresh()
        displayed = clipboard.entries
        guard displayed.count > 1 else { return }
        selected = 0
        firstRow = 0
        showingSettings = false
        buildSelector()
    }
    private func buildSelector(keepPosition: Bool = false) {
        guard !displayed.isEmpty else { hide(); return }
        let oldFrame = keepPosition ? popup?.frame : nil
        let visible = min(4, displayed.count)
        firstRow = min(max(firstRow, 0), max(0, displayed.count - visible))
        let height = 25 + rowHeight * CGFloat(visible)
        let panel = makePopup(height: height)
        let root = baseView(height: height)
        rows.removeAll()
        for local in 0..<visible {
            let absolute = firstRow + local
            let row = ClipboardRow(index: absolute, text: displayed[absolute], width: width - 10, height: rowHeight)
            row.frame.origin = NSPoint(x: 5, y: 5 + CGFloat(local) * rowHeight)
            row.onSelect = { [weak self] index in self?.selectRow(index) }
            row.onPaste = { [weak self] index in self?.choose(index) }
            row.onDelete = { [weak self] index in self?.delete(index) }
            root.addSubview(row)
            rows.append(row)
        }
        let hint = NSTextField(labelWithString: displayed.count > 4 ? "\(firstRow+1)–\(firstRow+visible)/\(displayed.count)" : "1 = última")
        hint.textColor = NSColor(calibratedWhite: 0.67, alpha: 1)
        hint.font = NSFont.systemFont(ofSize: 9)
        hint.alignment = .right
        hint.frame = NSRect(x: width - 96, y: height - 20, width: 85, height: 14)
        root.addSubview(hint)
        let header = NSTextField(labelWithString: "copy4paste")
        header.font = NSFont.systemFont(ofSize: 10, weight: .medium)
        header.textColor = NSColor(calibratedWhite: 0.70, alpha: 1)
        header.frame = NSRect(x: 10, y: height - 20, width: 130, height: 14)
        root.addSubview(header)
        panel.contentView = root
        updateSelected()
        if let old = oldFrame {
            panel.setFrame(NSRect(x: old.minX, y: old.maxY - height, width: width, height: height), display: true)
            panel.makeKeyAndOrderFront(nil)
        } else {
            present(panel, height: height)
        }
        scheduleDismiss()
    }

    private func selectRow(_ index: Int) {
        selected = index
        updateSelected()
        scheduleDismiss()
    }
    private func updateSelected() {
        for row in rows { row.isSelected = row.index == selected }
    }
    private func move(_ delta: Int) {
        guard !displayed.isEmpty else { return }
        selected = (selected + delta + displayed.count) % displayed.count
        if selected < firstRow { firstRow = selected; buildSelector(keepPosition: true) }
        else if selected >= firstRow + min(4, displayed.count) {
            firstRow = selected - min(4, displayed.count) + 1
            buildSelector(keepPosition: true)
        } else { updateSelected() }
        scheduleDismiss()
    }
    private func delete(_ index: Int) {
        guard displayed.indices.contains(index) else { return }
        clipboard.remove(at: index)
        displayed = clipboard.entries
        selected = min(selected, max(displayed.count - 1, 0))
        if displayed.count < 2 { hide(); return }
        buildSelector(keepPosition: true)
    }
    private func scroll(_ delta: Int) {
        guard displayed.count > 4 else { return }
        let next = min(max(firstRow + delta, 0), displayed.count - 4)
        guard next != firstRow else { return }
        firstRow = next
        selected = next
        buildSelector(keepPosition: true)
    }
    private func choose(_ index: Int) {
        guard clipboard.select(at: index) else { hide(); return }
        let previous = target
        hide(restoreFocus: false)
        _ = previous?.activate(options: .activateIgnoringOtherApps)
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.15) {
            let src = CGEventSource(stateID: .hidSystemState)
            let keyDown = CGEvent(keyboardEventSource: src, virtualKey: 9, keyDown: true)
            let keyUp = CGEvent(keyboardEventSource: src, virtualKey: 9, keyDown: false)
            for event in [keyDown, keyUp] {
                event?.flags = .maskCommand
                event?.setIntegerValueField(.eventSourceUserData, value: syntheticPasteMarker)
                event?.post(tap: .cghidEventTap)
            }
        }
    }
    private func hide(restoreFocus: Bool = true) {
        dismissTimer?.invalidate()
        popup?.orderOut(nil)
        popup = nil
        rows.removeAll()
        displayed.removeAll()
        showingSettings = false
        if restoreFocus { _ = target?.activate(options: .activateIgnoringOtherApps) }
        target = nil
    }
    private func scheduleDismiss() {
        dismissTimer?.invalidate()
        dismissTimer = Timer.scheduledTimer(withTimeInterval: 12, repeats: false) { [weak self] _ in
            guard let self = self, !self.showingSettings else { return }
            self.hide()
        }
    }

    private func openSettings() {
        if popup?.isVisible == true { hide() }
        showingSettings = true
        target = NSWorkspace.shared.frontmostApplication
        let height: CGFloat = 126
        let panel = makePopup(height: height)
        let root = baseView(height: height)
        let title = NSTextField(labelWithString: "copy4paste · Ajustes")
        title.font = NSFont.systemFont(ofSize: 12, weight: .medium)
        title.textColor = .white
        title.frame = NSRect(x: 12, y: 12, width: 230, height: 20)
        root.addSubview(title)
        let limit = NSTextField(labelWithString: "Copias: \(clipboard.capacity)")
        limit.identifier = NSUserInterfaceItemIdentifier("capacityLabel")
        limit.font = NSFont.systemFont(ofSize: 12)
        limit.textColor = .white
        limit.frame = NSRect(x: 12, y: 45, width: 150, height: 20)
        root.addSubview(limit)
        let stepper = NSStepper(frame: NSRect(x: 192, y: 43, width: 60, height: 24))
        stepper.minValue = Double(Config.minimum)
        stepper.maxValue = Double(Config.maximum)
        stepper.integerValue = clipboard.capacity
        stepper.target = self
        stepper.action = #selector(capacityChanged(_:))
        root.addSubview(stepper)
        let status = NSTextField(labelWithString: "⌘V · seleccionar   |   Esc · cerrar")
        status.font = NSFont.systemFont(ofSize: 10)
        status.textColor = NSColor(calibratedWhite: 0.72, alpha: 1)
        status.frame = NSRect(x: 12, y: 79, width: 258, height: 18)
        root.addSubview(status)
        let close = NSButton(title: "Cerrar", target: self, action: #selector(closeSettings(_:)))
        close.frame = NSRect(x: width - 92, y: height - 28, width: 80, height: 22)
        root.addSubview(close)
        panel.contentView = root
        present(panel, height: height)
    }
    @objc private func capacityChanged(_ sender: NSStepper) {
        Config.setCapacity(sender.integerValue)
        clipboard.changeCapacity(sender.integerValue)
        if let root = popup?.contentView,
           let label = root.subviews.compactMap({ $0 as? NSTextField }).first(where: { $0.identifier?.rawValue == "capacityLabel" }) {
            label.stringValue = "Copias: \(sender.integerValue)"
        }
    }
    @objc private func closeSettings(_ sender: Any?) { hide() }

    /// Auto-test interno: ejercita el selector real sin depender de permisos de Accesibilidad.
    /// Se ejecuta con `--selftest` en el runner macOS antes de empaquetar.
    func runSelfTest() {
        func check(_ condition: @autoclosure () -> Bool, _ label: String) {
            if !condition() {
                fputs("FAIL: \(label)\n", stderr)
                exit(1)
            }
            print("ok · \(label)")
        }
        func spin(_ seconds: TimeInterval = 0.25) {
            RunLoop.current.run(until: Date().addingTimeInterval(seconds))
        }
        func pasteEvent() -> CGEvent {
            let event = CGEvent(keyboardEventSource: nil, virtualKey: 9, keyDown: true)!
            event.flags = .maskCommand
            return event
        }
        let pasteboard = NSPasteboard.general

        // 1. La preferencia queda acotada a 1...30.
        Config.setCapacity(99)
        check(Config.capacity == 30, "preferencia se acota a 30")
        Config.setCapacity(0)
        check(Config.capacity == 1, "preferencia se acota a 1")
        Config.setCapacity(4)
        check(Config.capacity == 4, "preferencia vuelve a 4")

        // 2. Un solo texto: ⌘V pasa sin tocarse (pegado normal).
        pasteboard.clearContents()
        pasteboard.setString("solo uno", forType: .string)
        clipboard.refresh()
        check(clipboard.entries.count == 1, "historial con un texto")
        check(handle(type: .keyDown, event: pasteEvent()) != nil, "con un texto ⌘V no se intercepta")
        spin(0.1)
        check(popup == nil, "con un texto no se abre selector")

        // 3. Dos textos: ⌘V se intercepta y abre el selector.
        pasteboard.clearContents()
        pasteboard.setString("alfa", forType: .string)
        clipboard.refresh()
        pasteboard.clearContents()
        pasteboard.setString("beta", forType: .string)
        clipboard.refresh()
        check(clipboard.entries == ["beta", "alfa"], "orden: última copia primero")
        check(handle(type: .keyDown, event: pasteEvent()) == nil, "con dos textos ⌘V se intercepta")
        spin(0.4)
        check(popup?.isVisible == true, "el selector aparece")
        check(rows.count == 2, "una fila por copia")
        check(rows[0].isSelected && !rows[1].isSelected, "la primera fila nace sombreada")
        for row in rows {
            check(row.subviews.compactMap { $0 as? DeleteControl }.count == 1, "cada fila tiene su cruz")
        }

        // 4. Flecha abajo mueve la selección y el sombreado.
        popup?.onMove?(1)
        check(selected == 1, "flecha abajo selecciona la segunda")
        check(rows[1].isSelected && !rows[0].isSelected, "el sombreado sigue a la selección")
        popup?.onMove?(-1)
        check(selected == 0, "flecha arriba vuelve a la primera")

        // 5. Borrar una copia que NO es la del portapapeles no pega ni la altera.
        popup?.onMove?(1)
        let before = pasteboard.string(forType: .string)
        popup?.onDelete?()
        check(pasteboard.string(forType: .string) == before, "borrar no pega ni cambia el portapapeles")
        check(clipboard.entries == ["beta"], "la copia se borró del historial")
        check(popup == nil, "con una sola copia el selector se cierra")

        // 6. Elegir una copia la deja en el portapapeles (ruta de pegado).
        pasteboard.clearContents()
        pasteboard.setString("gamma", forType: .string)
        clipboard.refresh()
        check(clipboard.entries == ["gamma", "beta"], "historial listo para elegir")
        check(clipboard.select(at: 1), "elegir la segunda copia")
        check(pasteboard.string(forType: .string) == "beta", "la copia elegida llega al portapapeles")

        hide()
        check(popup == nil, "cerrar deja la app sin ventana")
        print("copy4paste SELFTEST OK: interceptación, filas, cruces, sombreado, borrado, pegado, preferencia")
    }
}

private func eventTapCallback(_ proxy: CGEventTapProxy, _ type: CGEventType,
                              _ event: CGEvent, _ ref: UnsafeMutableRawPointer?) -> Unmanaged<CGEvent>? {
    guard let ref = ref else { return Unmanaged.passUnretained(event) }
    return Unmanaged<Copy4PasteApp>.fromOpaque(ref).takeUnretainedValue().handle(type: type, event: event)
}

if CommandLine.arguments.contains("--version") {
    print("copy4paste 1.0-rc · runtime")
} else if CommandLine.arguments.contains("--selftest") {
    _ = NSApplication.shared
    Copy4PasteApp().runSelfTest()
} else if CommandLine.arguments.contains("--verify-ui") {
    // Smoke test ejecutada EN MAC por GitHub Actions, antes de distribuir la app.
    _ = NSApplication.shared
    let row = ClipboardRow(index: 0, text: "Última copia", width: 280, height: 31)
    let deletions = row.subviews.compactMap { $0 as? DeleteControl }
    precondition(deletions.count == 1, "Debe existir exactamente una cruz")
    let delete = deletions[0]
    precondition(delete.frame.minX > 200 && delete.frame.maxX <= row.bounds.maxX, "Cruz fuera de la fila")
    let crossPoint = NSPoint(x: delete.frame.midX, y: delete.frame.midY)
    precondition(row.hitTest(crossPoint) === delete, "La cruz no recibe clics")
    let labelPoint = NSPoint(x: 50, y: 15)
    precondition(row.hitTest(labelPoint) === row, "El texto bloquea los clics")
    row.isSelected = true
    precondition(row.isSelected, "La selección no se mantiene")
    var deleted = false
    row.onDelete = { index in deleted = (index == 0) }
    delete.onDelete?()
    precondition(deleted, "Borrado no conectado")
    print("copy4paste UI OK: x visible, clic, selección, borrado")
} else {
    let application = NSApplication.shared
    let delegate = Copy4PasteApp()
    application.delegate = delegate
    application.run()
}
