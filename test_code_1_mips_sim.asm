.data
val1: .word 5
val2: .word 10
result: .word 0

.text
start:
    addi $t7, $zero, 0        # Loop counter i = 0
    addi $t8, $zero, 5        # Loop limit 5

loop:
    lw $t0, val1              # Load val1
    lw $t1, val2              # Load val2
    add $t2, $t0, $t1         # val1 + val2

    lw $t3, result            # Load previous result
    add $t2, $t2, $t3         # Add new sum to previous result

    sw $t2, result            # Store accumulated result

    addi $t7, $t7, 1          # i++
    beq $t7, $t8, print       # If i == 5, jump to print

    j loop                    # Else continue loop

print:
    lw $a0, result            # Load accumulated result to $a0
    addi $v0, $zero, 1        # Syscall code for print integer
    syscall                   # Print the result

exit:
    addi $v0, $zero, 10       # Syscall code for exit
    syscall                   # Exit program
