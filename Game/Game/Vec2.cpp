#include "Vec2.h"
#include <cmath>
Vec2 Vec2::normalized() const {
	double length = std::sqrt(sq_Length());
	if (length == 0) {
		return Vec2(0, 0); // Avoid division by zero
	}
	return Vec2(x / length, y / length);
}
double Vec2::arcAngle() {
	return std::atan2(getY(), getX());
}
//从极坐标创建二维向量，将极坐标（角度和长度）转换为笛卡尔坐标系（x, y）
		Vec2 Vec2::fromPolar(double angle, double length)
{
	return Vec2(length * std::sin(angle),   // x = len * sin
		length * std::cos(angle));  // y = len * cos
}