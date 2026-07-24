#pragma once
class Vec2 //注:Y向下为+ , X向右为正 ,左上 0,0 起始生效
{

	
public:
	double x;
	double y;
	Vec2(double x, double y) : x(x), y(y) {}
	~Vec2() {};

	Vec2() : x(0), y(0) {}
	double getX() const { return x; }
	double getY() const { return y; }
	void setX(double x) { this->x = x; }
	void setY(double y) { this->y = y; }
	Vec2 operator+(const Vec2& other) const
	{
		return Vec2(x + other.x, y + other.y);
	}
	Vec2 operator-(const Vec2& other) const
	{
		return Vec2(x - other.x, y - other.y);
	}
	Vec2 operator*(double scalar) const
	{
		return Vec2(x * scalar, y * scalar);
	}
	Vec2 operator/(double scalar) const
	{
		return Vec2(x / scalar, y / scalar);
	}	
	
	double sq_Length() const {  // 计算向量的**平方**长度
		return x * x + y * y;
	}
	Vec2 normalized() const ; // 返回单位向量.	
	
	static Vec2 fromPolar(double angle, double lenth); // 从角度创建向量

	double arcAngle() ;



	Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
	Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }

	


};

