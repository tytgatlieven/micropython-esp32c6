# test memoryview accessing maximum values for signed/unsigned elements
try:
    memoryview
except:
    print("SKIP")
    raise SystemExit

# 32 bit platform slice offset 24 bit max
slice_max = 0xFFFFFF

try:
    buff = b"0123456789AB" * (slice_max // 10)
except MemoryError:
    print("SKIP")
    raise SystemExit
mv = memoryview(buff)


print(mv[slice_max : slice_max] == buff[slice_max : slice_max])

try:
    # On 64 bit this passes just fine
    print(mv[slice_max + 1 : slice_max + 10] == buff[slice_max + 1 : slice_max + 10])
except ValueError:
    # On 32 bit this raises ValueError for out-of-bound start address
    print(True)
