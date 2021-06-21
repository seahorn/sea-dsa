//#include "seahorn/seahorn.h"
class BaseClass {
public:
  BaseClass() {}
  virtual ~BaseClass() {}
  virtual int Func() { return 0; }
};
class DerivedClass : public BaseClass {
public:
  DerivedClass() {}
  int Func() { return 1; }
};
int main(int argc, char **argv) {
  BaseClass *bc = new DerivedClass();

  // Devirtualization should lower the virtual call to either a call
  // to BaseClass::Func or DerivedClass::Func. Then, the rest of
  // optimizations should exclude the call to BaseClass::Func.
  
  //sassert(bc->Func() == 1);
  return bc->Func();
}
