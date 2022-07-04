# test __float__ function support

class TestFloat():
    def __float__(self):
        return 10.0


class Test():
    pass


class TestFloatByIndex():
    '''When converting to float, if __float__ is not availble, fall back to using __index__'''
    def __index__(self):
        return 10  # int is correct here (it will be promoted to float during conversion)


class TestFloatByBadIndex():
    '''When converting to float, if falling back to __index__, an int *must* be returned'''
    def __index__(self):
        return "Raise TypeError"


class TestFloatWithUnusedIndex():
    def __float__(self):
        return 4.0
    def __index__(self):
        assert False  # When converting to float,  __index__ should be unused if __float__ is present

print("%.1f" % float(TestFloat()))
print("%.1f" % float(TestFloatByIndex()))
print("%.1f" % float(TestFloatWithUnusedIndex()))

try:
    print(float(Test()))
except TypeError:
    print("TypeError")

try:
    print(float(TestFloatByBadIndex()))
except TypeError:
    print("TypeError")
