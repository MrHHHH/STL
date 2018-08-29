#include "Vector.h"

int main()
{
	Vector<int> v1;
	v1.PushBack(1);
	v1.PushBack(2);
	v1.PushBack(3);
	v1.PushBack(4);
	v1.PushBack(5);

	PrintVector(v1);
	v1.PopBack();
	PrintVector(v1);
	return 0;
}
