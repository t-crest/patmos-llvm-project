---
format:          pml-0.1
triple:          patmos-unknown-unknown-elf
machine-functions:
  - name:            1
    level:           machinecode
    mapsto:          main
    arguments:
      - name:            '%x'
        index:           0
        registers:
          - r3
    hash:            0
    blocks:
      - name:            0
        mapsto:          entry
        predecessors:    [  ]
        successors:      [  ]
        instructions:
          - index:           0
            opcode:          INLINEASM
            size:            52
          - index:           1
            opcode:          LIl
            size:            8
          - index:           2
            opcode:          INLINEASM
            size:            60
          - index:           3
            opcode:          RETND
            size:            4
            branch-type:     return
    subfunctions:
      - name:            0
        blocks:          [ 0 ]
...
---
format:          pml-0.1
triple:          patmos-unknown-unknown-elf
bitcode-functions:
  - name:            main
    level:           bitcode
    hash:            0
    blocks:
      - name:            entry
        predecessors:    []
        successors:      []
        instructions:
          - index:           0
            opcode:          call
            callees:
              - __any__
          - index:           1
            opcode:          call
            callees:
              - __any__
          - index:           2
            opcode:          ret
...
---
format:          pml-0.1
triple:          patmos-unknown-unknown-elf
relation-graphs:
  - src:
      function:        main
      level:           bitcode
    dst:
      function:        1
      level:           machinecode
    nodes:
      - name:            0
        type:            entry
        src-block:       entry
        dst-block:       0
        src-successors:  [ 1 ]
        dst-successors:  [ 1 ]
      - name:            1
        type:            exit
        src-block:       ''
        dst-block:       0
    status:          valid
...
