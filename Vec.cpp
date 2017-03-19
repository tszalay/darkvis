#include "Vec.h"

ostream& operator <<(ostream &os,const Vector3 &a)
{
      os<<"("<<a.x<<","<<a.y<<","<<a.z<<")"<<endl;
      return os;
}
