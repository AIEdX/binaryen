;; NOTE: Assertions have been generated by update_lit_checks.py --all-items and should not be edited.
;; NOTE: This test was ported using port_passes_tests_to_lit.py and could be cleaned up.

;; RUN: foreach %s %t wasm-opt --generate-dyncalls --all-features -S -o - | filecheck %s

(module
 ;; CHECK:      (type $i32_i32_i32_=>_none (func (param i32 i32 i32)))

 ;; CHECK:      (type $none_=>_i32 (func (result i32)))

 ;; CHECK:      (type $i32_=>_i64 (func (param i32) (result i64)))

 ;; CHECK:      (type $i32_=>_i32 (func (param i32) (result i32)))

 ;; CHECK:      (type $i32_i32_=>_i64 (func (param i32 i32) (result i64)))

 ;; CHECK:      (type $i32_i32_=>_none (func (param i32 i32)))

 ;; CHECK:      (import "env" "invoke_vii" (func $invoke_vii (param i32 i32 i32)))
 (import "env" "invoke_vii" (func $invoke_vii (param i32 i32 i32)))
 ;; CHECK:      (table $0 2 2 funcref)

 ;; CHECK:      (elem (i32.const 0) $f1 $f2)

 ;; CHECK:      (export "dynCall_i" (func $dynCall_i))

 ;; CHECK:      (export "dynCall_ji" (func $dynCall_ji))

 ;; CHECK:      (export "dynCall_vii" (func $dynCall_vii))

 ;; CHECK:      (func $f1 (type $none_=>_i32) (result i32)
 ;; CHECK-NEXT:  (i32.const 1024)
 ;; CHECK-NEXT: )
 (func $f1 (result i32)
  (i32.const 1024)
 )
 ;; CHECK:      (func $f2 (type $i32_=>_i64) (param $0 i32) (result i64)
 ;; CHECK-NEXT:  (i64.const 42)
 ;; CHECK-NEXT: )
 (func $f2 (param i32) (result i64)
  (i64.const 42)
 )
 (table 2 2 funcref)
 (elem (i32.const 0) $f1 $f2)
)
;; CHECK:      (func $dynCall_i (type $i32_=>_i32) (param $fptr i32) (result i32)
;; CHECK-NEXT:  (call_indirect $0 (type $none_=>_i32)
;; CHECK-NEXT:   (local.get $fptr)
;; CHECK-NEXT:  )
;; CHECK-NEXT: )

;; CHECK:      (func $dynCall_ji (type $i32_i32_=>_i64) (param $fptr i32) (param $0 i32) (result i64)
;; CHECK-NEXT:  (call_indirect $0 (type $i32_=>_i64)
;; CHECK-NEXT:   (local.get $0)
;; CHECK-NEXT:   (local.get $fptr)
;; CHECK-NEXT:  )
;; CHECK-NEXT: )

;; CHECK:      (func $dynCall_vii (type $i32_i32_i32_=>_none) (param $fptr i32) (param $0 i32) (param $1 i32)
;; CHECK-NEXT:  (call_indirect $0 (type $i32_i32_=>_none)
;; CHECK-NEXT:   (local.get $0)
;; CHECK-NEXT:   (local.get $1)
;; CHECK-NEXT:   (local.get $fptr)
;; CHECK-NEXT:  )
;; CHECK-NEXT: )
(module
 ;; CHECK:      (type $i32_i32_i32_=>_none (func (param i32 i32 i32)))

 ;; CHECK:      (type $none_=>_i32 (func (result i32)))

 ;; CHECK:      (type $i32_=>_i32 (func (param i32) (result i32)))

 ;; CHECK:      (type $i32_i32_=>_none (func (param i32 i32)))

 ;; CHECK:      (import "env" "table" (table $timport$0 1 1 funcref))
 (import "env" "invoke_vii" (func $invoke_vii (param i32 i32 i32)))
 ;; CHECK:      (import "env" "invoke_vii" (func $invoke_vii (param i32 i32 i32)))
 (import "env" "table" (table 1 1 funcref))
 (elem (i32.const 0) $f)
 ;; CHECK:      (elem (i32.const 0) $f)

 ;; CHECK:      (export "dynCall_i" (func $dynCall_i))

 ;; CHECK:      (export "dynCall_vii" (func $dynCall_vii))

 ;; CHECK:      (func $f (type $none_=>_i32) (result i32)
 ;; CHECK-NEXT:  (i32.const 42)
 ;; CHECK-NEXT: )
 (func $f (result i32)
  (i32.const 42)
 )
)
;; CHECK:      (func $dynCall_i (type $i32_=>_i32) (param $fptr i32) (result i32)
;; CHECK-NEXT:  (call_indirect $timport$0 (type $none_=>_i32)
;; CHECK-NEXT:   (local.get $fptr)
;; CHECK-NEXT:  )
;; CHECK-NEXT: )

;; CHECK:      (func $dynCall_vii (type $i32_i32_i32_=>_none) (param $fptr i32) (param $0 i32) (param $1 i32)
;; CHECK-NEXT:  (call_indirect $timport$0 (type $i32_i32_=>_none)
;; CHECK-NEXT:   (local.get $0)
;; CHECK-NEXT:   (local.get $1)
;; CHECK-NEXT:   (local.get $fptr)
;; CHECK-NEXT:  )
;; CHECK-NEXT: )
