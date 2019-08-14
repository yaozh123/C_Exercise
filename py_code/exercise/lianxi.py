N = int(raw_input())
for i in range(int(N)):
    Q = raw_input()
    while True:
        if Q in T:
            Q += 1
        else:
            print Q
            break
