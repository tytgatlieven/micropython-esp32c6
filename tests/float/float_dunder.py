# test __float__ function support

class TestFloat():
    def __float__(self):
        return 10.0


class Test():
    pass


print("%.1f" % float(TestFloat()))

try:
    print(float(Test()))
except TypeError:
    print("TypeError")
