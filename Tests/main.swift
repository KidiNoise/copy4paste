import Foundation

func check(_ value: @autoclosure () -> Bool, _ label: String) {
    if !value() { fputs("FAIL: \(label)\n", stderr); exit(1) }
}

var history = HistoryStore()
check(history.capacity == 4, "cuatro entradas por defecto")
check(history.remember("primero"), "copia 1")
check(history.remember("segundo"), "copia 2")
check(history.remember("tercero"), "copia 3")
check(history.remember("cuarto"), "copia 4")
check(history.entries == ["cuarto", "tercero", "segundo", "primero"], "orden nuevo primero")
check(history.remember("quinto"), "quinta copia")
check(history.entries == ["quinto", "cuarto", "tercero", "segundo"], "capacidad cuatro")
check(history.remember("tercero"), "duplicados")
check(history.entries == ["tercero", "quinto", "cuarto", "segundo"], "duplicado vuelve al principio")
check(history.remove("quinto"), "borrar entrada")
check(history.entries == ["tercero", "cuarto", "segundo"], "orden tras borrar")
check(!history.remove("inexistente"), "borrar inexistente")
history.setCapacity(1)
check(history.entries == ["tercero"], "reducir capacidad")
history.setCapacity(30)
check(history.remember("nuevo"), "ampliar capacidad")
check(history.entries == ["nuevo", "tercero"], "ampliación sin perder historial")
check(!history.remember(""), "no guardar vacío")
check(!history.remember(String(repeating: "x", count: 2_000_001)), "límite de bytes")
print("OK: 18 comprobaciones del historial")
