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
print("OK: 18 history checks")
