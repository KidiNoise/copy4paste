import Foundation

func check(_ value: @autoclosure () -> Bool, _ label: String) {
    if !value() { fputs("FAIL: \(label)\n", stderr); exit(1) }
}

var history = HistoryStore()
check(history.capacity == 4, "four entries by default")
check(history.remember("primero"), "copy 1")
check(history.remember("segundo"), "copy 2")
check(history.remember("tercero"), "copy 3")
check(history.remember("cuarto"), "copy 4")
check(history.entries == ["cuarto", "tercero", "segundo", "primero"], "newest first")
check(history.remember("quinto"), "fifth copy")
check(history.entries == ["quinto", "cuarto", "tercero", "segundo"], "capacity of four")
check(history.remember("tercero"), "duplicates")
check(history.entries == ["tercero", "quinto", "cuarto", "segundo"], "duplicate moves to the front")
check(history.remove("quinto"), "delete entry")
check(history.entries == ["tercero", "cuarto", "segundo"], "order after delete")
check(!history.remove("inexistente"), "delete missing entry")
history.setCapacity(1)
check(history.entries == ["tercero"], "shrink capacity")
history.setCapacity(30)
check(history.remember("nuevo"), "grow capacity")
check(history.entries == ["nuevo", "tercero"], "growth keeps history")
check(!history.remember(""), "do not store empty")
check(!history.remember(String(repeating: "x", count: 2_000_001)), "byte limit")

var skinChecks = 0
func skinCheck(_ value: @autoclosure () -> Bool, _ label: String) {
    skinChecks += 1
    check(value(), label)
}

// --- SkinColor hex parsing ---
let opaque = SkinColor(hex: "#FFFFFF")!
skinCheck(opaque.red == 1.0 && opaque.green == 1.0 && opaque.blue == 1.0 && opaque.alpha == 1.0, "#RRGGBB parses opaque white")
let noHash = SkinColor(hex: "000000")
skinCheck(noHash != nil && noHash!.red == 0.0, "hex without # parses")
let transparent = SkinColor(hex: "#FF000080")
skinCheck(transparent != nil && abs(transparent!.alpha - 128.0/255.0) < 0.0001, "#RRGGBBAA parses alpha")
skinCheck(SkinColor(hex: "#FFF") == nil, "rejects 3-digit hex")
skinCheck(SkinColor(hex: "#GGGGGG") == nil, "rejects non-hex")
skinCheck(SkinColor(hex: "") == nil, "rejects empty hex")

// --- Built-in skins ---
skinCheck(SkinTheme.builtins.count == 4, "four built-in skins")
skinCheck(SkinTheme.builtins.map { $0.name } == ["Dark", "Light", "Cyan", "Midnight"], "built-in names")
skinCheck(SkinTheme.defaultID == "dark", "default id is dark")
skinCheck(SkinTheme.builtins.first!.isLight == false, "dark is not light")
skinCheck(SkinTheme.builtins[1].isLight == true, "light is light")

// --- JSON decoding ---
func skinData(_ dict: [String: Any]) -> Data {
    try! JSONSerialization.data(withJSONObject: dict, options: [])
}
let valid: [String: Any] = [
    "name": "Sunset",
    "author": "Jane",
    "colors": [
        "background": "#1E2A38",
        "selection": "#3B82F6D9",
        "foreground": "#F8FAFC",
        "muted": "#94A3B8",
        "delete": "#F1F5F9",
    ],
    "appearance": "light",
    "cornerRadius": 12,
    "font": ["name": "Menlo", "size": 13],
]
let sunset = try! SkinTheme(jsonData: skinData(valid), id: "sunset")
skinCheck(sunset.name == "Sunset" && sunset.author == "Jane", "decodes name and author")
skinCheck(sunset.appearance == .light && sunset.isLight, "decodes appearance")
skinCheck(sunset.cornerRadius == 12, "decodes corner radius")
skinCheck(sunset.font.name == "Menlo" && sunset.font.size == 13, "decodes font")
skinCheck(sunset.background.red > 0.1, "decodes background color")

let defaults: [String: Any] = ["name": "Minimal", "colors": [
    "background": "#000000", "selection": "#111111", "foreground": "#222222",
    "muted": "#333333", "delete": "#444444",
]]
let minimal = try! SkinTheme(jsonData: skinData(defaults), id: "minimal")
skinCheck(minimal.appearance == .dark && minimal.cornerRadius == 9, "defaults to dark and radius 9")
skinCheck(minimal.font.name == nil && minimal.font.size == nil, "defaults to system font")

do {
    _ = try SkinTheme(jsonData: skinData(["colors": ["background": "#000000"]]), id: "x")
    skinCheck(false, "missing name throws")
} catch let error as SkinError { skinCheck(error == .missingName, "missing name error") }

do {
    _ = try SkinTheme(jsonData: skinData(["name": "X"]), id: "x")
    skinCheck(false, "missing colors throws")
} catch let error as SkinError { skinCheck(error == .missingColors, "missing colors error") }

do {
    _ = try SkinTheme(jsonData: skinData(["name": "X", "colors": ["background": "#000000"]]), id: "x")
    skinCheck(false, "missing color throws")
} catch let error as SkinError { skinCheck(error == .missingColor("selection"), "missing selection error") }

do {
    _ = try SkinTheme(jsonData: skinData(["name": "X", "colors": [
        "background": "#000000", "selection": "#111111", "foreground": "#222222",
        "muted": "not-a-color", "delete": "#444444",
    ]]), id: "x")
    skinCheck(false, "invalid color throws")
} catch let error as SkinError { skinCheck(error == .invalidColor("muted"), "invalid muted error") }

do {
    _ = try SkinTheme(jsonData: skinData(["name": "X", "colors": [
        "background": "#000000", "selection": "#111111", "foreground": "#222222",
        "muted": "#333333", "delete": "#444444",
    ], "appearance": "blue"]), id: "x")
    skinCheck(false, "invalid appearance throws")
} catch let error as SkinError { skinCheck(error == .invalidAppearance("blue"), "invalid appearance error") }

do {
    _ = try SkinTheme(jsonData: skinData(["name": "X", "colors": [
        "background": "#000000", "selection": "#111111", "foreground": "#222222",
        "muted": "#333333", "delete": "#444444",
    ], "cornerRadius": 999]), id: "x")
    skinCheck(false, "invalid radius throws")
} catch let error as SkinError { skinCheck(error == .invalidCornerRadius, "invalid radius error") }

// --- Legacy id mapping ---
skinCheck(SkinLibrary.legacyID(rawValue: 0) == "dark", "legacy 0 is dark")
skinCheck(SkinLibrary.legacyID(rawValue: 1) == "light", "legacy 1 is light")
skinCheck(SkinLibrary.legacyID(rawValue: 2) == "cyan", "legacy 2 is cyan")
skinCheck(SkinLibrary.legacyID(rawValue: 3) == "midnight", "legacy 3 is midnight")
skinCheck(SkinLibrary.legacyID(rawValue: 99) == "dark", "unknown legacy falls back to dark")

// --- Identifier sanitisation ---
skinCheck(SkinLibrary.safeIdentifier("sunset.json") == "sunset", "strips .json")
skinCheck(SkinLibrary.safeIdentifier("my skin") == "my skin", "keeps spaces")
skinCheck(SkinLibrary.safeIdentifier("../evil") == "evil", "removes path separators")
skinCheck(SkinLibrary.safeIdentifier("///") == "skin", "empty name becomes skin")

// --- Library: resolve, install, import, fallback ---
let dir = FileManager.default.temporaryDirectory
    .appendingPathComponent("copy4paste-skin-tests-\(UUID().uuidString)", isDirectory: true)
let library = SkinLibrary(skinsDirectory: dir)
try! FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)

skinCheck(library.resolve(id: nil).id == "dark", "resolve nil falls back to dark")
skinCheck(library.resolve(id: "cyan").id == "cyan", "resolve built-in by id")
skinCheck(library.resolve(id: "does-not-exist").id == "dark", "resolve unknown falls back to dark")

let source = dir.appendingPathComponent("sunset.json")
try! skinData(valid).write(to: source)
let imported = try! library.importSkin(from: source)
skinCheck(imported.id == "sunset", "import uses the file name as id")
skinCheck(imported.name == "Sunset", "import keeps the display name")
skinCheck(library.resolve(id: "sunset").id == "sunset", "resolve imported skin by id")
skinCheck(library.installedSkins().count == 1, "one installed skin")
skinCheck(library.allSkins().count == 5, "four built-ins plus one installed")

let collisionSource = dir.appendingPathComponent("dark.json")
try! skinData(valid).write(to: collisionSource)
let collided = try! library.importSkin(from: collisionSource)
skinCheck(collided.id == "dark-custom", "built-in name is renamed on import")

let broken = dir.appendingPathComponent("broken.json")
try! "not json".data(using: .utf8)!.write(to: broken)
skinCheck(library.resolve(id: "broken").id == "dark", "broken installed skin falls back")
try? FileManager.default.removeItem(at: dir)

print("OK: 18 history checks · \(skinChecks) skin checks")
