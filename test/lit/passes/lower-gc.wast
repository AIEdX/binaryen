;; NOTE: Assertions have been generated by update_lit_checks.py and should not be edited.
;; RUN: wasm-opt %s --lower-gc -all -S -o - \
;; RUN:   | filecheck %s

(module
 (type $empty (struct))
 (type $struct-i32 (struct (mut i32)))
 (type $struct-i64 (struct (mut i64)))
 (type $struct-f32 (struct (field (mut f32))))
 (type $struct-f64 (struct (field (mut f64))))
 (type $struct-ref (struct (field (mut (ref null $empty)))))
 (type $struct-rtt (struct (field (mut (rtt $empty)))))

 ;; CHECK:      (func $loads (param $ref-i32 i32) (param $ref-i64 i32) (param $ref-f32 i32) (param $ref-f64 i32) (param $ref-ref i32) (param $ref-rtt i32)
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (i32.load offset=4
 ;; CHECK-NEXT:    (local.get $ref-i32)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (i64.load offset=4
 ;; CHECK-NEXT:    (local.get $ref-i64)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (f32.load offset=4
 ;; CHECK-NEXT:    (local.get $ref-f32)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (f64.load offset=4
 ;; CHECK-NEXT:    (local.get $ref-f64)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (i32.load offset=4
 ;; CHECK-NEXT:    (local.get $ref-ref)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (drop
 ;; CHECK-NEXT:   (i32.load offset=4
 ;; CHECK-NEXT:    (local.get $ref-rtt)
 ;; CHECK-NEXT:   )
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $loads
  (param $ref-i32 (ref $struct-i32))
  (param $ref-i64 (ref $struct-i64))
  (param $ref-f32 (ref $struct-f32))
  (param $ref-f64 (ref $struct-f64))
  (param $ref-ref (ref $struct-ref))
  (param $ref-rtt (ref $struct-rtt))
  (drop
   (struct.get $struct-i32 0 (local.get $ref-i32))
  )
  (drop
   (struct.get $struct-i64 0 (local.get $ref-i64))
  )
  (drop
   (struct.get $struct-f32 0 (local.get $ref-f32))
  )
  (drop
   (struct.get $struct-f64 0 (local.get $ref-f64))
  )
  (drop
   (struct.get $struct-ref 0 (local.get $ref-ref))
  )
  (drop
   (struct.get $struct-rtt 0 (local.get $ref-rtt))
  )
 )
 ;; CHECK:      (func $stores (param $ref-i32 i32) (param $ref-i64 i32) (param $ref-f32 i32) (param $ref-f64 i32) (param $ref-ref i32) (param $ref-rtt i32)
 ;; CHECK-NEXT:  (i32.store offset=4
 ;; CHECK-NEXT:   (local.get $ref-i32)
 ;; CHECK-NEXT:   (i32.const 0)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (i64.store offset=4
 ;; CHECK-NEXT:   (local.get $ref-i64)
 ;; CHECK-NEXT:   (i64.const 0)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (f32.store offset=4
 ;; CHECK-NEXT:   (local.get $ref-f32)
 ;; CHECK-NEXT:   (f32.const 0)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (f64.store offset=4
 ;; CHECK-NEXT:   (local.get $ref-f64)
 ;; CHECK-NEXT:   (f64.const 0)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (i32.store offset=4
 ;; CHECK-NEXT:   (local.get $ref-ref)
 ;; CHECK-NEXT:   (i32.const 0)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT:  (i32.store offset=4
 ;; CHECK-NEXT:   (local.get $ref-rtt)
 ;; CHECK-NEXT:   (i32.const 0)
 ;; CHECK-NEXT:  )
 ;; CHECK-NEXT: )
 (func $stores
  (param $ref-i32 (ref $struct-i32))
  (param $ref-i64 (ref $struct-i64))
  (param $ref-f32 (ref $struct-f32))
  (param $ref-f64 (ref $struct-f64))
  (param $ref-ref (ref $struct-ref))
  (param $ref-rtt (ref $struct-rtt))
  (struct.set $struct-i32 0 (local.get $ref-i32) (i32.const 0))
  (struct.set $struct-i64 0 (local.get $ref-i64) (i64.const 0))
  (struct.set $struct-f32 0 (local.get $ref-f32) (f32.const 0))
  (struct.set $struct-f64 0 (local.get $ref-f64) (f64.const 0))
  (struct.set $struct-ref 0 (local.get $ref-ref) (ref.null $empty))
  (struct.set $struct-rtt 0 (local.get $ref-rtt) (rtt.canon $empty))
 )
)
