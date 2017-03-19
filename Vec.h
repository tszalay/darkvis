#ifndef _VEC_HH_
#define _VEC_HH_

#include <cmath>
#include <iostream>

using namespace std;

struct __attribute__ ((__packed__)) Vector3
{
    float x,y,z;

    Vector3() {	x=y=z=0; }

    Vector3(float x, float y, float z)
    { this->x = x; this->y = y;	this->z = z; }

    Vector3(const Vector3& a)
    { x=a.x; y=a.y; z=a.z; }

    Vector3& operator=(const Vector3 &rhs) { 
	x=rhs.x; y=rhs.y; z=rhs.z; 
	return *this;
    }

    Vector3& operator+=(const Vector3 &a) { 
	x+=a.x; y+=a.y; z+=a.z; 
	return *this;
    }

    Vector3& operator-=(const Vector3 &a) { 
	x-=a.x; y-=a.y; z-=a.z; 
	return *this;
    }

    Vector3& operator*=(const Vector3 &a) { 
	x*=a.x; y*=a.y; z*=a.z; 
	return *this;
    }

    Vector3& operator*=(float s) { 
	x*=s; y*=s; z*=s; 
	return *this;
    }

    const Vector3 operator+(const Vector3 &a) const {
	return Vector3(*this) += a;
    }

    const Vector3 operator-(const Vector3 &a) const {
	return (Vector3(*this) -= a);
    }

    const Vector3 operator*(float s) const {
	return Vector3(*this) *= s;
    }

    const Vector3 operator-() {
	return Vector3(-x,-y,-z);
    }



    /*** STATIC VECTOR OPERATIONS ***/

    static float SqLength(Vector3 a) {
	return a.x*a.x + a.y*a.y + a.z*a.z;
    }

    static float Length(Vector3 a) {
	return sqrt(a.x*a.x + a.y*a.y + a.z*a.z);
    }

    static Vector3 Normalize(Vector3 a) {
	return (a * (1.0f / Length(a)));
    }

    static float Dot(Vector3 a, Vector3& b) {
	return a.x*b.x + a.y*b.y + a.z*b.z;
    }

    static Vector3 Cross(Vector3 a, Vector3 b) {
	return Vector3(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);
    }

// for passing to openGL functions
    operator float*() {
	return (float*)this;
    }
};

// for debug output
ostream& operator <<(ostream &os,const Vector3 &a);

#endif
