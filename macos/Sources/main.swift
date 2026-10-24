import AppKit
import ApplicationServices
import CoreGraphics

// copy4paste: un portapapeles ligero. macOS 10.15+, Intel.
private enum Skin: Int, CaseIterable {
    case dark = 0
    case light = 1
    case cyan = 2
    case midnight = 3

    var title: String {
        switch self {
        case .dark: return "Oscuro"
        case .light: return "Claro"
        case .cyan: return "Cian"
        case .midnight: return "Medianoche"
        }
    }
    var background: NSColor {
        switch self {
        case .dark: return NSColor(calibratedRed: 0.15, green: 0.15, blue: 0.17, alpha: 1)
        case .light: return NSColor(calibratedRed: 0.96, green: 0.96, blue: 0.97, alpha: 1)
        case .cyan: return NSColor(calibratedRed: 0.03, green: 0.14, blue: 0.18, alpha: 1)
        case .midnight: return NSColor(calibratedRed: 0.05, green: 0.07, blue: 0.16, alpha: 1)
        }
    }
    var selection: NSColor {
        switch self {
        case .dark: return NSColor(calibratedWhite: 1, alpha: 0.18)
        case .light: return NSColor(calibratedRed: 0.55, green: 0.65, blue: 0.82, alpha: 0.55)
        case .cyan: return NSColor(calibratedRed: 0.05, green: 0.45, blue: 0.55, alpha: 0.85)
        case .midnight: return NSColor(calibratedRed: 0.18, green: 0.25, blue: 0.55, alpha: 0.85)
        }
    }
    var foreground: NSColor {
        switch self {
        case .light: return NSColor(calibratedWhite: 0.10, alpha: 1)
        default: return NSColor(calibratedWhite: 0.96, alpha: 1)
        }
    }
    var muted: NSColor {
        switch self {
        case .light: return NSColor(calibratedWhite: 0.42, alpha: 1)
        case .dark: return NSColor(calibratedWhite: 0.67, alpha: 1)
        case .cyan: return NSColor(calibratedRed: 0.55, green: 0.75, blue: 0.80, alpha: 1)
        case .midnight: return NSColor(calibratedRed: 0.58, green: 0.63, blue: 0.78, alpha: 1)
        }
    }
    var delete: NSColor {
        switch self {
        case .light: return NSColor(calibratedWhite: 0.35, alpha: 1)
        case .dark: return NSColor(calibratedWhite: 0.86, alpha: 1)
        case .cyan: return NSColor(calibratedRed: 0.70, green: 0.95, blue: 1.0, alpha: 1)
        case .midnight: return NSColor(calibratedRed: 0.78, green: 0.84, blue: 1.0, alpha: 1)
        }
    }
}

private enum Config {
    static let capacityKey = "copy4paste.capacity"
    static let skinKey = "copy4paste.skin"
    static let opacityKey = "copy4paste.opacity"
    static let minimum = 1
    static let maximum = 30
    static let minOpacity = 20
    static let maxOpacity = 100
    static var capacity: Int {
        let raw = UserDefaults.standard.object(forKey: capacityKey) as? Int ?? 4
        return min(max(raw, minimum), maximum)
    }
    static func setCapacity(_ n: Int) {
        UserDefaults.standard.set(min(max(n, minimum), maximum), forKey: capacityKey)
    }
    static var skin: Skin {
        Skin(rawValue: UserDefaults.standard.integer(forKey: skinKey)) ?? .dark
    }
    static func setSkin(_ skin: Skin) {
        UserDefaults.standard.set(skin.rawValue, forKey: skinKey)
    }
    static var opacity: Int {
        let raw = UserDefaults.standard.object(forKey: opacityKey) as? Int ?? 94
        return min(max(raw, minOpacity), maxOpacity)
    }
    static func setOpacity(_ n: Int) {
        UserDefaults.standard.set(min(max(n, minOpacity), maxOpacity), forKey: opacityKey)
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
        Config.skin.delete.setStroke()
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
        let theme = Config.skin
        let number = NSTextField(labelWithString: "\(index + 1)")
        number.font = NSFont.monospacedDigitSystemFont(ofSize: 11, weight: index == 0 ? .semibold : .regular)
        number.textColor = theme.muted
        number.frame = NSRect(x: 9, y: 7, width: 18, height: 18)
        addSubview(number)
        let preview = text.replacingOccurrences(of: "\n", with: " ↵ ")
                          .replacingOccurrences(of: "\r", with: " ")
                          .replacingOccurrences(of: "\t", with: " ⇥ ")
        let field = NSTextField(labelWithString: preview)
        field.font = NSFont.systemFont(ofSize: 12)
        field.textColor = theme.foreground
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
            Config.skin.selection.setFill()
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
    var isSettings = false
    override func scrollWheel(with event: NSEvent) {
        if isSettings { super.scrollWheel(with: event); return }
        if abs(event.scrollingDeltaY) > 0.25 {
            onScroll?(event.scrollingDeltaY < 0 ? 1 : -1)
        } else { super.scrollWheel(with: event) }
    }
    override var canBecomeKey: Bool { true }
    override var canBecomeMain: Bool { false }
    override func keyDown(with event: NSEvent) {
        if isSettings {
            if event.keyCode == 53 { onEscape?() } else { super.keyDown(with: event) }
            return
        }
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

    func remove(text: String) {
        _ = store.remove(text)
        if clipboard.string(forType: .string) == text {
            clipboard.clearContents()
            if let first = entries.first { clipboard.setString(first, forType: .string) }
            observed = clipboard.changeCount
            isText = !entries.isEmpty
        }
    }
    func select(text: String) -> Bool {
        guard !text.isEmpty else { return false }
        clipboard.clearContents()
        clipboard.setString(text, forType: .string)
        observed = clipboard.changeCount
        isText = true
        // No reordenamos al pegar: el orden muestra las últimas copias reales.
        return true
    }
}

private let syntheticPasteMarker: Int64 = 0xC0F4A57E

private final class Copy4PasteApp: NSObject, NSApplicationDelegate, NSTextFieldDelegate {
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
        if CommandLine.arguments.contains("--verify-settings") {
            NSApp.setActivationPolicy(.accessory)
            let skin = Config.skin
            let bg = skin.background.withAlphaComponent(CGFloat(Config.opacity) / 100.0)
            let log = "skin=\(skin.rawValue) opacity=\(Config.opacity) bg=\(bg) fg=\(skin.foreground)\n"
            try? log.write(toFile: "/tmp/c4p-settings-config.log", atomically: true, encoding: .utf8)
            openSettings()
            DispatchQueue.main.asyncAfter(deadline: .now() + 12) { NSApp.terminate(nil) }
            return
        }
        if CommandLine.arguments.contains("--verify-selector") {
            NSApp.setActivationPolicy(.accessory)
            let pasteboard = NSPasteboard.general
            pasteboard.clearContents()
            pasteboard.setString("Ejemplo de primera copia", forType: .string)
            clipboard.refresh()
            pasteboard.clearContents()
            pasteboard.setString("Ejemplo de segunda copia", forType: .string)
            clipboard.refresh()
            openSelector()
            DispatchQueue.main.asyncAfter(deadline: .now() + 12) { NSApp.terminate(nil) }
            return
        }
        if CommandLine.arguments.contains("--verify-runtime") {
            beginVerification()
            return
        }
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

    private func beginVerification() {
        NSApp.setActivationPolicy(.accessory)
        var lines: [String] = []
        func flush() {
            let text = lines.joined(separator: "\n") + "\n"
            try? text.write(toFile: "/tmp/copy4paste-verify.log", atomically: true, encoding: .utf8)
        }
        lines.append("AX_TRUSTED=\(AXIsProcessTrusted())")
        let pasteboard = NSPasteboard.general
        pasteboard.clearContents()
        pasteboard.setString("VERIFY-UNO", forType: .string)
        clipboard.refresh()
        pasteboard.clearContents()
        pasteboard.setString("VERIFY-DOS", forType: .string)
        clipboard.refresh()
        lines.append("ENTRIES=\(clipboard.entries)")
        lines.append("IS_TEXT=\(clipboard.isText)")
        startTap()
        lines.append("TAP_CREATED=\(keyboardTap != nil)")
        flush()
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) {
            let source = CGEventSource(stateID: .hidSystemState)
            let down = CGEvent(keyboardEventSource: source, virtualKey: 9, keyDown: true)
            let up = CGEvent(keyboardEventSource: source, virtualKey: 9, keyDown: false)
            for event in [down, up] {
                event?.flags = .maskCommand
                event?.post(tap: .cghidEventTap)
            }
            lines.append("POSTED_CMD_V=\(down != nil && up != nil)")
            flush()
        }
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) {
            lines.append("POPUP_VISIBLE=\(self.popup?.isVisible == true)")
            lines.append("DISPLAYED=\(self.displayed)")
            lines.append("ROWS=\(self.rows.count)")
            flush()
        }
        DispatchQueue.main.asyncAfter(deadline: .now() + 8) {
            NSApp.terminate(nil)
        }
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
        applyBackground(to: view)
        return view
    }

    private func applyBackground(to view: NSView) {
        let alpha = CGFloat(Config.opacity) / 100.0
        view.layer?.backgroundColor = Config.skin.background.withAlphaComponent(alpha).cgColor
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
        let height = 28 + rowHeight * CGFloat(visible)
        let panel = makePopup(height: height)
        let root = baseView(height: height)
        rows.removeAll()
        for local in 0..<visible {
            let absolute = firstRow + local
            let row = ClipboardRow(index: absolute, text: displayed[absolute], width: width - 10, height: rowHeight)
            row.frame.origin = NSPoint(x: 5, y: 25 + CGFloat(local) * rowHeight)
            row.onSelect = { [weak self] index in self?.selectRow(index) }
            row.onPaste = { [weak self] index in self?.choose(index) }
            row.onDelete = { [weak self] index in self?.delete(index) }
            root.addSubview(row)
            rows.append(row)
        }
        let hint = NSTextField(labelWithString: displayed.count > 4 ? "\(firstRow+1)–\(firstRow+visible)/\(displayed.count)" : "1 = última")
        hint.textColor = Config.skin.muted
        hint.font = NSFont.systemFont(ofSize: 9)
        hint.alignment = .right
        hint.frame = NSRect(x: width - 136, y: 6, width: 92, height: 14)
        root.addSubview(hint)
        let header = NSTextField(labelWithString: "copy4paste")
        header.font = NSFont.systemFont(ofSize: 10, weight: .medium)
        header.textColor = Config.skin.muted
        header.frame = NSRect(x: 10, y: 6, width: 120, height: 14)
        root.addSubview(header)
        let gear = NSButton(frame: NSRect(x: width - 36, y: 3, width: 26, height: 20))
        gear.isBordered = false
        gear.target = self
        gear.action = #selector(openSettingsAction(_:))
        gear.toolTip = "Configuración (⌃⌥⌘P)"
        gear.attributedTitle = NSAttributedString(string: "⚙", attributes: [
            .font: NSFont.systemFont(ofSize: 13),
            .foregroundColor: Config.skin.muted
        ])
        root.addSubview(gear)
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
        clipboard.remove(text: displayed[index])
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
        guard displayed.indices.contains(index),
              clipboard.select(text: displayed[index]) else { hide(); return }
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

    private func footerButton(_ title: String, frame: NSRect, action: Selector, size: CGFloat = 11) -> NSButton {
        let button = NSButton(frame: frame)
        button.isBordered = false
        button.target = self
        button.action = action
        button.attributedTitle = NSAttributedString(string: title, attributes: [
            .font: NSFont.systemFont(ofSize: size, weight: .medium),
            .foregroundColor: Config.skin.muted
        ])
        return button
    }

    private func openSettings(preserveFrame: Bool = false, preserveTarget: Bool = false) {
        let oldFrame = preserveFrame ? popup?.frame : nil
        let oldTarget = target
        if popup?.isVisible == true { hide() }
        showingSettings = true
        target = preserveTarget ? oldTarget : NSWorkspace.shared.frontmostApplication
        let height: CGFloat = 218
        let panel = makePopup(height: height)
        panel.isSettings = true
        let root = baseView(height: height)
        let theme = Config.skin

        let title = NSTextField(labelWithString: "copy4paste · Ajustes · 1.1")
        title.font = NSFont.systemFont(ofSize: 12, weight: .semibold)
        title.textColor = theme.foreground
        title.frame = NSRect(x: 14, y: 12, width: 262, height: 20)
        root.addSubview(title)

        let limitLabel = NSTextField(labelWithString: "Copias")
        limitLabel.font = NSFont.systemFont(ofSize: 12)
        limitLabel.textColor = theme.foreground
        limitLabel.frame = NSRect(x: 14, y: 52, width: 100, height: 20)
        root.addSubview(limitLabel)
        let limitValue = NSTextField(labelWithString: "\(clipboard.capacity)")
        limitValue.identifier = NSUserInterfaceItemIdentifier("capacityLabel")
        limitValue.font = NSFont.monospacedDigitSystemFont(ofSize: 13, weight: .medium)
        limitValue.textColor = theme.foreground
        limitValue.alignment = .right
        limitValue.frame = NSRect(x: 150, y: 52, width: 34, height: 20)
        root.addSubview(limitValue)
        let stepper = NSStepper(frame: NSRect(x: 196, y: 50, width: 52, height: 24))
        stepper.minValue = Double(Config.minimum)
        stepper.maxValue = Double(Config.maximum)
        stepper.integerValue = clipboard.capacity
        stepper.target = self
        stepper.action = #selector(capacityChanged(_:))
        stepper.appearance = NSAppearance(named: theme == .light ? .aqua : .darkAqua)
        root.addSubview(stepper)

        let skinLabel = NSTextField(labelWithString: "Skin")
        skinLabel.font = NSFont.systemFont(ofSize: 12)
        skinLabel.textColor = theme.foreground
        skinLabel.frame = NSRect(x: 14, y: 92, width: 80, height: 20)
        root.addSubview(skinLabel)
        let skinPopup = NSPopUpButton(frame: NSRect(x: 100, y: 90, width: 176, height: 24), pullsDown: false)
        skinPopup.addItems(withTitles: Skin.allCases.map { $0.title })
        skinPopup.selectItem(at: Config.skin.rawValue)
        skinPopup.target = self
        skinPopup.action = #selector(skinChanged(_:))
        skinPopup.controlSize = .small
        skinPopup.font = NSFont.systemFont(ofSize: 11)
        skinPopup.appearance = NSAppearance(named: theme == .light ? .aqua : .darkAqua)
        root.addSubview(skinPopup)

        let opacityLabel = NSTextField(labelWithString: "Opacidad")
        opacityLabel.font = NSFont.systemFont(ofSize: 12)
        opacityLabel.textColor = theme.foreground
        opacityLabel.frame = NSRect(x: 14, y: 132, width: 100, height: 20)
        root.addSubview(opacityLabel)
        let opacityField = NSTextField(frame: NSRect(x: 150, y: 131, width: 50, height: 22))
        opacityField.identifier = NSUserInterfaceItemIdentifier("opacityField")
        opacityField.stringValue = "\(Config.opacity)"
        opacityField.font = NSFont.monospacedDigitSystemFont(ofSize: 12, weight: .medium)
        opacityField.alignment = .right
        opacityField.isEditable = true
        opacityField.isBezeled = true
        opacityField.bezelStyle = .roundedBezel
        opacityField.target = self
        opacityField.action = #selector(opacityChanged(_:))
        opacityField.delegate = self
        opacityField.appearance = NSAppearance(named: theme == .light ? .aqua : .darkAqua)
        root.addSubview(opacityField)
        let percent = NSTextField(labelWithString: "%")
        percent.font = NSFont.systemFont(ofSize: 12)
        percent.textColor = theme.muted
        percent.frame = NSRect(x: 203, y: 132, width: 20, height: 20)
        root.addSubview(percent)

        let help = NSTextField(labelWithString: "4 copias por defecto · 1–30 · historial solo en RAM")
        help.font = NSFont.systemFont(ofSize: 9)
        help.textColor = theme.muted
        help.frame = NSRect(x: 14, y: 166, width: 262, height: 16)
        root.addSubview(help)

        let quit = footerButton("Salir", frame: NSRect(x: width - 176, y: height - 32, width: 76, height: 22),
                                action: #selector(quitApp(_:)))
        root.addSubview(quit)
        let close = footerButton("Cerrar", frame: NSRect(x: width - 92, y: height - 32, width: 80, height: 22),
                                 action: #selector(closeSettings(_:)))
        root.addSubview(close)

        panel.contentView = root
        if let old = oldFrame {
            panel.setFrame(NSRect(x: old.minX, y: old.maxY - height, width: width, height: height), display: true)
            panel.makeKeyAndOrderFront(nil)
        } else {
            present(panel, height: height)
        }
    }

    @objc private func openSettingsAction(_ sender: Any?) { openSettings() }

    @objc private func capacityChanged(_ sender: NSStepper) {
        let value = min(max(sender.integerValue, Config.minimum), Config.maximum)
        Config.setCapacity(value)
        clipboard.changeCapacity(value)
        if let root = popup?.contentView,
           let label = root.subviews.compactMap({ $0 as? NSTextField }).first(where: { $0.identifier?.rawValue == "capacityLabel" }) {
            label.stringValue = "\(value)"
        }
    }

    @objc private func skinChanged(_ sender: NSPopUpButton) {
        Config.setSkin(Skin(rawValue: sender.indexOfSelectedItem) ?? .dark)
        openSettings(preserveFrame: true, preserveTarget: true)
    }

    @objc private func opacityChanged(_ sender: Any?) {
        guard let field = sender as? NSTextField else { return }
        applyOpacity(from: field)
    }

    func controlTextDidEndEditing(_ obj: Notification) {
        guard let field = obj.object as? NSTextField,
              field.identifier?.rawValue == "opacityField" else { return }
        applyOpacity(from: field)
    }

    private func applyOpacity(from field: NSTextField) {
        let text = field.stringValue.trimmingCharacters(in: .whitespacesAndNewlines)
        guard let value = Int(text) else {
            field.stringValue = "\(Config.opacity)"
            return
        }
        let clamped = min(max(value, Config.minOpacity), Config.maxOpacity)
        Config.setOpacity(clamped)
        field.stringValue = "\(clamped)"
        if let root = popup?.contentView {
            applyBackground(to: root)
        }
    }

    @objc private func closeSettings(_ sender: Any?) {
        let previous = target
        guard clipboard.entries.count > 1 else {
            hide()
            return
        }
        hide(restoreFocus: false)
        target = previous
        openSelector()
    }

    @objc private func quitApp(_ sender: Any?) { NSApp.terminate(nil) }
}

private func eventTapCallback(_ proxy: CGEventTapProxy, _ type: CGEventType,
                              _ event: CGEvent, _ ref: UnsafeMutableRawPointer?) -> Unmanaged<CGEvent>? {
    guard let ref = ref else { return Unmanaged.passUnretained(event) }
    return Unmanaged<Copy4PasteApp>.fromOpaque(ref).takeUnretainedValue().handle(type: type, event: event)
}

if CommandLine.arguments.contains("--version") {
    print("copy4paste 1.1 · runtime")
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
