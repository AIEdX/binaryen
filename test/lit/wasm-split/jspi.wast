;; NOTE: Assertions have been generated by update_lit_checks.py --all-items and should not be edited.
;; RUN: wasm-opt %s --jspi --pass-arg=jspi-exports@foo --pass-arg=jspi-split-module -all -S -o %t.jspi.wast
;; RUN: wasm-split %t.jspi.wast --export-prefix='%' -g -o1 %t.1.wasm -o2 %t.2.wasm --keep-funcs=foo --jspi --enable-reference-types
;; RUN: wasm-dis %t.1.wasm | filecheck %s --check-prefix PRIMARY
;; RUN: wasm-dis %t.2.wasm | filecheck %s --check-prefix SECONDARY

;; Check that the call to bar first checks if the secondary module is loaded and
;; that bar is moved to the secondary module.

(module
 ;; PRIMARY:      (type $i32_=>_i32 (func (param i32) (result i32)))

 ;; PRIMARY:      (type $externref_=>_none (func (param externref)))

 ;; PRIMARY:      (type $externref_i32_=>_i32 (func (param externref i32) (result i32)))

 ;; PRIMARY:      (type $none_=>_none (func))

 ;; PRIMARY:      (import "env" "__load_secondary_module" (func $import$__load_secondary_module (param externref)))

 ;; PRIMARY:      (import "placeholder" "0" (func $placeholder_0 (param i32) (result i32)))

 ;; PRIMARY:      (global $suspender (mut externref) (ref.null noextern))

 ;; PRIMARY:      (global $global$1 (mut i32) (i32.const 0))

 ;; PRIMARY:      (table $0 1 funcref)

 ;; PRIMARY:      (elem $0 (i32.const 0) $placeholder_0)

 ;; PRIMARY:      (export "foo" (func $export$foo))
 (export "foo" (func $foo))
 ;; PRIMARY:      (export "load_secondary_module_status" (global $global$1))

 ;; PRIMARY:      (export "%foo" (func $foo))

 ;; PRIMARY:      (export "%table" (table $0))

 ;; PRIMARY:      (export "%global" (global $suspender))

 ;; PRIMARY:      (func $foo (param $0 i32) (result i32)
 ;; PRIMARY-NEXT:  (if
 ;; PRIMARY-NEXT:   (i32.eqz
 ;; PRIMARY-NEXT:    (global.get $global$1)
 ;; PRIMARY-NEXT:   )
 ;; PRIMARY-NEXT:   (call $__load_secondary_module)
 ;; PRIMARY-NEXT:  )
 ;; PRIMARY-NEXT:  (call_indirect (type $i32_=>_i32)
 ;; PRIMARY-NEXT:   (i32.const 0)
 ;; PRIMARY-NEXT:   (i32.const 0)
 ;; PRIMARY-NEXT:  )
 ;; PRIMARY-NEXT: )
 (func $foo (param i32) (result i32)
  (call $bar (i32.const 0))
 )
 ;; SECONDARY:      (type $i32_=>_i32 (func (param i32) (result i32)))

 ;; SECONDARY:      (import "primary" "%table" (table $timport$0 1 funcref))

 ;; SECONDARY:      (import "primary" "%global" (global $suspender (mut externref)))

 ;; SECONDARY:      (import "primary" "load_secondary_module_status" (global $gimport$1 (mut i32)))

 ;; SECONDARY:      (import "primary" "%foo" (func $foo (param i32) (result i32)))

 ;; SECONDARY:      (elem $0 (i32.const 0) $bar)

 ;; SECONDARY:      (func $bar (param $0 i32) (result i32)
 ;; SECONDARY-NEXT:  (call $foo
 ;; SECONDARY-NEXT:   (i32.const 1)
 ;; SECONDARY-NEXT:  )
 ;; SECONDARY-NEXT: )
 (func $bar (param i32) (result i32)
  (call $foo (i32.const 1))
 )
)

;; PRIMARY:      (func $export$foo (param $susp externref) (param $0 i32) (result i32)
;; PRIMARY-NEXT:  (global.set $suspender
;; PRIMARY-NEXT:   (local.get $susp)
;; PRIMARY-NEXT:  )
;; PRIMARY-NEXT:  (call $foo
;; PRIMARY-NEXT:   (local.get $0)
;; PRIMARY-NEXT:  )
;; PRIMARY-NEXT: )

;; PRIMARY:      (func $__load_secondary_module
;; PRIMARY-NEXT:  (local $0 externref)
;; PRIMARY-NEXT:  (local.set $0
;; PRIMARY-NEXT:   (global.get $suspender)
;; PRIMARY-NEXT:  )
;; PRIMARY-NEXT:  (call $import$__load_secondary_module
;; PRIMARY-NEXT:   (global.get $suspender)
;; PRIMARY-NEXT:  )
;; PRIMARY-NEXT:  (global.set $suspender
;; PRIMARY-NEXT:   (local.get $0)
;; PRIMARY-NEXT:  )
;; PRIMARY-NEXT: )
