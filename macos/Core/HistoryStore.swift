import Foundation

/// Text history that lives only in memory: no files, no external services.
struct HistoryStore {
    private(set) var entries: [String] = []
    private(set) var capacity: Int
    let maximumBytes: Int

    init(capacity: Int = 4, maximumBytes: Int = 2_000_000) {
        precondition(capacity > 0 && maximumBytes > 0)
        self.capacity = capacity
        self.maximumBytes = maximumBytes
    }

    /// Changing the capacity trims the current history without writing to disk.
    mutating func setCapacity(_ newCapacity: Int) {
        precondition(newCapacity > 0)
        capacity = newCapacity
        trim()
    }

    @discardableResult
    mutating func remember(_ text: String) -> Bool {
        guard !text.isEmpty, text.utf8.count <= maximumBytes else { return false }
        entries.removeAll { $0 == text }
        entries.insert(text, at: 0)
        trim()
        return true
    }

    /// Remove a single copy without disturbing the order of the rest.
    @discardableResult
    mutating func remove(_ text: String) -> Bool {
        guard let index = entries.firstIndex(of: text) else { return false }
        entries.remove(at: index)
        return true
    }

    private mutating func trim() {
        if entries.count > capacity { entries.removeLast(entries.count - capacity) }
    }
}
