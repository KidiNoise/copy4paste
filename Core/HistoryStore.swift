import Foundation

/// Historial de texto residente solo en memoria: sin ficheros ni servicios externos.
struct HistoryStore {
    private(set) var entries: [String] = []
    private(set) var capacity: Int
    let maximumBytes: Int

    init(capacity: Int = 4, maximumBytes: Int = 2_000_000) {
        precondition(capacity > 0 && maximumBytes > 0)
        self.capacity = capacity
        self.maximumBytes = maximumBytes
    }

    /// Cambiar capacidad trunca el historial actual sin copiarlo a disco.
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

    /// Eliminar una copia concreta sin modificar el orden de las demás.
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
