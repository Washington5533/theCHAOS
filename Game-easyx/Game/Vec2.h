#pragma once
#include <cmath>

class Vec2 // 注:Y向下为+ , X向右为正 , 左上 0,0 起始生效
{
public:
	double x, y;

	Vec2(double x, double y) : x(x), y(y) {}
	explicit Vec2() : x(0), y(0) {}
	~Vec2() {}

	double getX() const { return x; }
	double getY() const { return y; }
	void setX(double v) { x = v; }
	void setY(double v) { y = v; }

	Vec2 operator+(const Vec2& o) const { return Vec2(x + o.x, y + o.y); }
	Vec2 operator-(const Vec2& o) const { return Vec2(x - o.x, y - o.y); }
	Vec2 operator*(double s)    const { return Vec2(x * s, y * s); }
	Vec2 operator/(double s)    const { return Vec2(x / s, y / s); }
	Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
	Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }

	double sq_Length() const { return x*x + y*y; }
	double length()    const { return std::sqrt(sq_Length()); }

	Vec2 normalized() const {
		double len = length();
		if (len == 0) return Vec2(0, 0);
		return Vec2(x / len, y / len);
	}

	double arcAngle() const { return std::atan2(y, x); }

	static Vec2 fromPolar(double angle, double len) {
		return Vec2(len * std::sin(angle), len * std::cos(angle));
	}
};
