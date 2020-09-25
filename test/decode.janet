(import ../build/_janet_bare1 :as _bare)

(assert (= (_bare/decode @{} 'u8 @"\x00") 0))
(assert (= (_bare/decode @{} 'u8 @"\x01") 1))
(assert (= (_bare/decode @{} 'u8 @"\xff") 255))

# variable uint
(assert (= (_bare/decode @{} 'uint @"\x00") 0))
(assert (= (_bare/decode @{} 'uint @"\x01") 1))
(assert (= (_bare/decode @{} 'uint @"\x80\x01") 128))
(assert (= (_bare/decode @{} 'uint @"\x80\x80\x01") 16384))

# variable uint/u64
(assert (= (_bare/decode @{} 'uint/u64 @"\x00") (int/u64 0)))
(assert (= (_bare/decode @{} 'uint/u64 @"\x01") (int/u64 1)))
(assert (= (_bare/decode @{} 'uint/u64 @"\x80\x01") (int/u64 128)))
(assert (= (_bare/decode @{} 'uint/u64 @"\x80\x80\x01") (int/u64 16384)))

# data
(assert (deep= (_bare/decode @{} 'data @"\x01\x55") @"\x55"))

# named type
(assert (deep= (_bare/decode @{:main 'u8} :main @"\x03") 3))

# struct
(assert (deep= (_bare/decode @{} '(struct :foo u8) @"\x03") @{:foo 3}))

# map
(assert (deep= (_bare/decode @{} '(map string u8) @"\x02\x01x\x03\x01y\x04") @{"x" 3 "y" 4}))

# optional
(assert (deep= (_bare/decode @{} '(optional u8) @"\x00") nil))
(assert (deep= (_bare/decode @{} '(optional u8) @"\x01\xff") 255))

# variable array
(assert (deep= (_bare/decode @{} '(array u8) @"\x00") @[]))
(assert (deep= (_bare/decode @{} '(array u8) @"\x02\x00\xff") @[0 255]))

# fixed array
(assert (deep= (_bare/decode @{} '(array u8) @"\x00") @[]))
(assert (deep= (_bare/decode @{} '(array 2 u8) @"\x00\xff") @[0 255]))

# strings
(assert (deep= 
          (_bare/decode @{} '(struct :a string :b keyword :c symbol) @"\x01x\x01x\x01x")
          @{:a "x" :b :x :c 'x}))



(do 
  (def schema '@{
    
    :customer
      (struct :name string
              :email (optional string)
              :metadata (map string string)
              :subscriptions (array :subscription))
      
    :subscription
      (struct :product string
              :plan uint)
  })

  (def buf @"\x06andrew\x00\x00\x01\x07dogfood\xff\x00")

  (assert (deep=
            (_bare/decode schema :customer buf)
            @{:name "andrew" :metadata @{} :subscriptions @[@{:product "dogfood" :plan 127}]})))