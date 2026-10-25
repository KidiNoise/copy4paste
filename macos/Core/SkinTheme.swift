import Foundation

/// A color as stored in a skin file. Foundation-only so the model can be
/// parsed and tested without AppKit; the app maps it to `NSColor` at the edge.
public struct SkinColor: Equatable {
    public var red: Double
    public var green: Double
    public var blue: Double
    public var alpha: Double

    public init(red: Double, green: Double, blue: Double, alpha: Double = 1.0) {
        self.red = red
        self.green = green
        self.blue = blue
        self.alpha = alpha
    }

    /// Parses `#RRGGBB` or `#RRGGBBAA` (the `#` is optional).
    public init?(hex: String) {
        var value = hex.trimmingCharacters(in: .whitespacesAndNewlines)
        if value.hasPrefix("#") { value.removeFirst() }
        let count = value.count
        guard count == 6 || count == 8 else { return nil }
        guard let packed = UInt64(value, radix: 16) else { return nil }
        switch count {
        case 6:
            self.init(red: Double((packed >> 16) & 0xFF) / 255.0,
                      green: Double((packed >> 8) & 0xFF) / 255.0,
                      blue: Double(packed & 0xFF) / 255.0,
                      alpha: 1.0)
        default:
            self.init(red: Double((packed >> 24) & 0xFF) / 255.0,
                      green: Double((packed >> 16) & 0xFF) / 255.0,
                      blue: Double((packed >> 8) & 0xFF) / 255.0,
                      alpha: Double(packed & 0xFF) / 255.0)
        }
    }
}

/// Whether a skin is designed for a dark or a light interface. This only picks
/// the right control appearance (stepper, pop-up, text field); it never
/// changes system-wide appearance.
public enum SkinAppearance: String, Equatable {
    case dark
    case light
}

/// Optional typography overrides. Missing values fall back to the system font.
public struct SkinFont: Equatable {
    public var name: String?
    public var size: Double?

    public init(name: String? = nil, size: Double? = nil) {
        self.name = name
        self.size = size
    }
}

public enum SkinError: Error, Equatable, CustomStringConvertible {
    case notADictionary
    case missingName
    case missingColors
    case missingColor(String)
    case invalidColor(String)
    case invalidAppearance(String)
    case invalidCornerRadius
    case invalidFont
    case notAFile

    public var description: String {
        switch self {
        case .notADictionary:
            return "The file must be a JSON object."
        case .missingName:
            return "The skin is missing a \"name\"."
        case .missingColors:
            return "The skin is missing its \"colors\" object."
        case .missingColor(let key):
            return "The skin is missing the color \"\(key)\"."
        case .invalidColor(let key):
            return "Color \"\(key)\" is not a valid #RRGGBB or #RRGGBBAA value."
        case .invalidAppearance(let value):
            return "\"appearance\" must be \"dark\" or \"light\" (got \"\(value)\")."
        case .invalidCornerRadius:
            return "\"cornerRadius\" must be a number between 0 and 30."
        case .invalidFont:
            return "\"font\" must be an object with an optional \"name\" and \"size\"."
        case .notAFile:
            return "The selected file could not be read."
        }
    }
}

/// A visual theme. Skins are pure data: JSON is parsed into these values and
/// nothing in a skin file is ever executed.
public struct SkinTheme: Equatable {
    public let id: String
    public let name: String
    public let author: String?
    public let background: SkinColor
    public let selection: SkinColor
    public let foreground: SkinColor
    public let muted: SkinColor
    public let delete: SkinColor
    public let appearance: SkinAppearance
    public let cornerRadius: Double
    public let font: SkinFont

    public var isLight: Bool { appearance == .light }

    public init(id: String, name: String, author: String?, background: SkinColor,
                selection: SkinColor, foreground: SkinColor, muted: SkinColor,
                delete: SkinColor, appearance: SkinAppearance, cornerRadius: Double,
                font: SkinFont) {
        self.id = id
        self.name = name
        self.author = author
        self.background = background
        self.selection = selection
        self.foreground = foreground
        self.muted = muted
        self.delete = delete
        self.appearance = appearance
        self.cornerRadius = cornerRadius
        self.font = font
    }

    public init(jsonData: Data, id: String) throws {
        let object: Any
        do {
            object = try JSONSerialization.jsonObject(with: jsonData, options: [])
        } catch {
            throw SkinError.notADictionary
        }
        guard let dictionary = object as? [String: Any] else { throw SkinError.notADictionary }
        try self.init(dictionary: dictionary, id: id)
    }

    public init(dictionary: [String: Any], id: String) throws {
        guard let name = dictionary["name"] as? String, !name.isEmpty else {
            throw SkinError.missingName
        }
        self.id = id
        self.name = name
        self.author = dictionary["author"] as? String

        guard let colors = dictionary["colors"] as? [String: Any] else {
            throw SkinError.missingColors
        }
        self.background = try SkinTheme.color(from: colors, key: "background")
        self.selection = try SkinTheme.color(from: colors, key: "selection")
        self.foreground = try SkinTheme.color(from: colors, key: "foreground")
        self.muted = try SkinTheme.color(from: colors, key: "muted")
        self.delete = try SkinTheme.color(from: colors, key: "delete")

        if let raw = dictionary["appearance"] as? String {
            guard let appearance = SkinAppearance(rawValue: raw) else {
                throw SkinError.invalidAppearance(raw)
            }
            self.appearance = appearance
        } else {
            self.appearance = .dark
        }

        if let raw = dictionary["cornerRadius"] {
            guard let number = raw as? NSNumber,
                  number.doubleValue >= 0, number.doubleValue <= 30 else {
                throw SkinError.invalidCornerRadius
            }
            self.cornerRadius = number.doubleValue
        } else {
            self.cornerRadius = 9
        }

        if let raw = dictionary["font"] {
            guard let fontDictionary = raw as? [String: Any] else {
                throw SkinError.invalidFont
            }
            var font = SkinFont()
            if let name = fontDictionary["name"] as? String, !name.isEmpty {
                font.name = name
            }
            if let size = fontDictionary["size"] as? NSNumber, size.doubleValue >= 8, size.doubleValue <= 36 {
                font.size = size.doubleValue
            }
            self.font = font
        } else {
            self.font = SkinFont()
        }
    }

    private static func color(from colors: [String: Any], key: String) throws -> SkinColor {
        guard let raw = colors[key] else { throw SkinError.missingColor(key) }
        guard let hex = raw as? String, let color = SkinColor(hex: hex) else {
            throw SkinError.invalidColor(key)
        }
        return color
    }
}

public extension SkinTheme {
    /// The id the app falls back to when the stored selection is missing or broken.
    static let defaultID = "dark"

    /// The four skins that have shipped with copy4paste since v1.1.
    static var builtins: [SkinTheme] {
        [
            SkinTheme(id: "dark", name: "Dark", author: nil,
                      background: SkinColor(red: 0.15, green: 0.15, blue: 0.17),
                      selection: SkinColor(red: 1.0, green: 1.0, blue: 1.0, alpha: 0.18),
                      foreground: SkinColor(red: 0.96, green: 0.96, blue: 0.96),
                      muted: SkinColor(red: 0.67, green: 0.67, blue: 0.67),
                      delete: SkinColor(red: 0.86, green: 0.86, blue: 0.86),
                      appearance: .dark, cornerRadius: 9, font: SkinFont()),
            SkinTheme(id: "light", name: "Light", author: nil,
                      background: SkinColor(red: 0.96, green: 0.96, blue: 0.97),
                      selection: SkinColor(red: 0.55, green: 0.65, blue: 0.82, alpha: 0.55),
                      foreground: SkinColor(red: 0.10, green: 0.10, blue: 0.10),
                      muted: SkinColor(red: 0.42, green: 0.42, blue: 0.42),
                      delete: SkinColor(red: 0.35, green: 0.35, blue: 0.35),
                      appearance: .light, cornerRadius: 9, font: SkinFont()),
            SkinTheme(id: "cyan", name: "Cyan", author: nil,
                      background: SkinColor(red: 0.03, green: 0.14, blue: 0.18),
                      selection: SkinColor(red: 0.05, green: 0.45, blue: 0.55, alpha: 0.85),
                      foreground: SkinColor(red: 0.96, green: 0.96, blue: 0.96),
                      muted: SkinColor(red: 0.55, green: 0.75, blue: 0.80),
                      delete: SkinColor(red: 0.70, green: 0.95, blue: 1.0),
                      appearance: .dark, cornerRadius: 9, font: SkinFont()),
            SkinTheme(id: "midnight", name: "Midnight", author: nil,
                      background: SkinColor(red: 0.05, green: 0.07, blue: 0.16),
                      selection: SkinColor(red: 0.18, green: 0.25, blue: 0.55, alpha: 0.85),
                      foreground: SkinColor(red: 0.96, green: 0.96, blue: 0.96),
                      muted: SkinColor(red: 0.58, green: 0.63, blue: 0.78),
                      delete: SkinColor(red: 0.78, green: 0.84, blue: 1.0),
                      appearance: .dark, cornerRadius: 9, font: SkinFont()),
        ]
    }
}

/// Loads the built-in skins plus any `.json` skins the user has imported.
public final class SkinLibrary {
    public let skinsDirectory: URL

    public init(skinsDirectory: URL) {
        self.skinsDirectory = skinsDirectory
    }

    /// `~/Library/Application Support/copy4paste/skins`.
    public static func applicationSkinsDirectory(fileManager: FileManager = .default) -> URL {
        let base = fileManager.urls(for: .applicationSupportDirectory, in: .userDomainMask).first
            ?? fileManager.homeDirectoryForCurrentUser.appendingPathComponent("Library/Application Support")
        return base.appendingPathComponent("copy4paste/skins", isDirectory: true)
    }

    public func builtins() -> [SkinTheme] { SkinTheme.builtins }

    /// Every imported skin, skipping (never crashing on) malformed files.
    public func installedSkins(fileManager: FileManager = .default) -> [SkinTheme] {
        let keys: [URLResourceKey] = [.isRegularFileKey]
        guard let urls = try? fileManager.contentsOfDirectory(at: skinsDirectory,
                                                               includingPropertiesForKeys: keys,
                                                               options: [.skipsHiddenFiles]) else {
            return []
        }
        return urls
            .filter { $0.pathExtension.lowercased() == "json" }
            .compactMap { url in
                guard let data = try? Data(contentsOf: url) else { return nil }
                return try? SkinTheme(jsonData: data, id: SkinLibrary.identifier(for: url))
            }
            .sorted { $0.name.localizedCaseInsensitiveCompare($1.name) == .orderedAscending }
    }

    public func allSkins(fileManager: FileManager = .default) -> [SkinTheme] {
        builtins() + installedSkins(fileManager: fileManager)
    }

    /// Resolves the stored id to a skin. Falls back to the default skin when
    /// the id is nil, unknown or points at a broken file.
    public func resolve(id: String?, fileManager: FileManager = .default) -> SkinTheme {
        if let id = id {
            if let builtin = builtins().first(where: { $0.id == id }) { return builtin }
            if let installed = installedSkins(fileManager: fileManager).first(where: { $0.id == id }) {
                return installed
            }
        }
        return builtins().first(where: { $0.id == SkinTheme.defaultID })
            ?? builtins()[0]
    }

    /// Copies a skin JSON into the skins directory and returns the installed theme.
    public func importSkin(from url: URL, fileManager: FileManager = .default) throws -> SkinTheme {
        guard let data = try? Data(contentsOf: url) else { throw SkinError.notAFile }
        var identifier = SkinLibrary.safeIdentifier(url.lastPathComponent)
        if builtins().contains(where: { $0.id == identifier }) {
            identifier += "-custom"
        }
        let theme = try SkinTheme(jsonData: data, id: identifier)
        try fileManager.createDirectory(at: skinsDirectory, withIntermediateDirectories: true, attributes: nil)
        let destination = skinsDirectory.appendingPathComponent(identifier).appendingPathExtension("json")
        try data.write(to: destination, options: .atomic)
        return theme
    }

    public static func identifier(for url: URL) -> String {
        url.deletingPathExtension().lastPathComponent
    }

    /// Turns any file name into a safe skin id (no path separators, no `.json`).
    public static func safeIdentifier(_ fileName: String) -> String {
        var name = fileName
        if name.lowercased().hasSuffix(".json") {
            name = String(name.dropLast(5))
        }
        let allowed = CharacterSet.alphanumerics.union(CharacterSet(charactersIn: "-_ "))
        let filtered = name.unicodeScalars
            .filter { allowed.contains($0) }
            .map { String($0) }
            .joined()
        let trimmed = filtered.trimmingCharacters(in: .whitespacesAndNewlines)
        return trimmed.isEmpty ? "skin" : trimmed
    }

    /// Maps the pre-v1.2 integer skin ids to the new string ids.
    public static func legacyID(rawValue: Int) -> String {
        switch rawValue {
        case 1: return "light"
        case 2: return "cyan"
        case 3: return "midnight"
        default: return "dark"
        }
    }
}
