
(declare-project
  :name "bare1"
  :description "A library providing janet encoders/decoders for the BARE format."
  :dependencies [])

(declare-native
  :name "_janet_bare1"
  :source ["_bare1.c"])

(declare-source
  :source ["bare1.janet"])