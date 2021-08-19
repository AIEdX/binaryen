;; NOTE: Assertions have been generated by update_lit_checks.py --all-items and should not be edited.

;; RUN: foreach %s %t wasm-opt --inlining -all --optimize-level=3 -S -o - | filecheck %s

;; Test the maySkipHeavyWork functionality in inlining: if a function has heavy
;; work, but we might actually avoid doing that work, then, it is still
;; potentially worth inlining.

(module
  (tag $exception (param i32))

  (import "outside" "work" (func $outside-work))

  (memory 1 1)

  (func $if-call (param $x i32)
    ;; An avoidable call.
    (if
      (local.get $x)
      (call $outside-work)
    )
  )

  (func $if-call-caller (param $x i32)
    ;; Call more than once, to avoid us deciding to inline just because the
    ;; target has a single use.
    (call $if-call (local.get $x))
    (call $if-call (local.get $x))
  )

  (func $if-fence (param $x i32)
    ;; An avoidable atomic fence, which is very costly.
    (if
      (local.get $x)
      (atomic.fence)
    )
  )

  (func $if-fence-caller (param $x i32)
    (call $if-fence (local.get $x))
    (call $if-fence (local.get $x))
  )

  (func $if-throw (param $x i32)
    ;; An avoidable throw, which is very costly.
    (if
      (local.get $x)
      (throw $exception (i32.const 0))
    )
  )

  (func $if-throw-caller (param $x i32)
    (call $if-throw (local.get $x))
    (call $if-throw (local.get $x))
  )

  (func $if-else (param $x i32)
    ;; As above, but in if-else form.
    (if
      (local.get $x)
      (nop)
      (call $outside-work)
    )
  )

  (func $if-else-caller (param $x i32)
    (call $if-else (local.get $x))
    (call $if-else (local.get $x))
  )

  (func $if-else-value (param $x i32) (result i32)
    ;; As above, but with a result
    (if (result i32)
      (local.get $x)
      (call $outside-work)
      (local.get $x)
    )
  )

  (func $if-else-caller (param $x i32)
    (drop (call $if-else-value (local.get $x)))
    (drop (call $if-else-value (local.get $x)))
  )

  (func $if-unreachable (param $x i32) (result i32)
    ;; The if arm does not return.
    (if
      (local.get $x)
      (throw $exception (i32.const 0))
    )
    (local.get $x)
  )

  (func $if-unreachable-caller (param $x i32)
    (drop (call $if-unreachable (local.get $x)))
    (drop (call $if-unreachable (local.get $x)))
  )


  (func $if-some-unavoidable (param $x i32)
    ;; Do a little work in the condition, but not too much.
    (if
      (i32.eqz (i32.eqz (local.get $x)))
      (call $outside-work)
    )
  )

  (func $if-some-unavoidable-caller (param $x i32)
    (call $if-some-unavoidable (local.get $x))
    (call $if-some-unavoidable (local.get $x))
  )

  (func $if-some-unavoidable-2 (param $x i32)
    ;; Do a little work in the condition, and a little after the if, but the
    ;; total is not too much.
    (if
      (i32.eqz (local.get $x))
      (call $outside-work)
    )
    (drop (i32.eqz (i32.const 0)))
  )

  (func $if-some-unavoidable-2-caller (param $x i32)
    (call $if-some-unavoidable-2 (local.get $x))
    (call $if-some-unavoidable-2 (local.get $x))
  )

  (func $if-null (param $x anyref) (result anyref
    ;; A "throw if null" function.
    (if
      (ref.is_null (local.get $x))
      (throw $exception (i32.const 0))
    )
    (local.get $x)
  )

  (func $if-null-caller (param $x i32)
    (call $if-null (local.get $x))
    (call $if-null (local.get $x))
  )
)
