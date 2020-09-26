(import ../build/_janet_bare1 :as _bare)

# variable uint
(assert (deep= (_bare/encode @{} 'uint 0) @"\x00"))
(assert (deep= (_bare/encode @{} 'uint 1) @"\x01"))
(assert (deep= (_bare/encode @{} 'uint 16384) @"\x80\x80\x01"))

# data
(assert (deep= (_bare/encode @{} 'data  @"\x55") @"\x01\x55"))

# named type
(assert (deep= (_bare/encode @{:main 'uint} :main 3) @"\x03"))

# struct
(assert (deep= (_bare/encode @{} '(struct :foo uint) @{:foo 3}) @"\x03"))

# map
(assert (deep= (_bare/encode @{} '(map string uint) @{"x" 3}) @"\x01\x01x\x03"))

# optional
(assert (deep= (_bare/encode @{} '(optional u8) nil)  @"\x00"))
(assert (deep= (_bare/encode @{} '(optional u8) 255) @"\x01\xff"))

# variable array
(assert (deep= (_bare/encode @{} '(array u8) @[]) @"\x00"))
(assert (deep= (_bare/encode @{} '(array u8) @[0 255]) @"\x02\x00\xff"))

# fixed array
(assert (deep= (_bare/encode @{} '(array u8) @[]) @"\x00"))
(assert (deep= (_bare/encode @{} '(array u8 2) @[0 255]) @"\x00\xff"))

# strings
(assert (deep= 
          (_bare/encode @{}  '(struct :a string :b keyword :c symbol) @{:a "x" :b :x :c 'x})
          @"\x01x\x01x\x01x"))

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

  (def customer @{:name "andrew" :metadata @{} :subscriptions @[@{:product "dogfood" :plan 127}]})

  (assert (deep=
            (_bare/encode schema :customer customer)
            @"\x06andrew\x00\x00\x01\x07dogfood\x7f")))