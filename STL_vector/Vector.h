#pragma once

#include "TypeTraits.h"
#include <cassert>
#include <iostream>
using namespace std;

//template <class T, class Ref,class Ptr>
//struct Iterator
//{
//	
//};

template <class T>
class Vector
{
public:
	typedef T* Iterator;
	typedef const T* ConstIterator;
	Vector()
		:start(NULL)
		,end(NULL)
		,end_of_storage()
	{}
	~Vector()
	{
		if(start!=NULL)
		{
			delete[] start;
			start = NULL;
		}
	}

	void PushBack(const T& data)
	{
		CheckCapacity();
		*end = data;
		end++;	
	}
	void PopBack()
	{
		assert(start != end);
		--end;
	}
	size_t Size()
	{
		return end-start;
	}
	Iterator Begin()
	{
		return start;
	}
	Iterator End()
	{
		return end;
	}
	ConstIterator Begin() const 
	{
		return start;
	}
	ConstIterator End() const 
	{
		return end;
	}
protected:
	void CheckCapacity()
	{
		if(end == end_of_storage)
		{
			size_t size = Size();
			size_t capacity = size*2+3;
			Iterator tmp = new T[capacity];
			if(TypeTraits<T>::IsPODType().Get())
			{
				memcpy(tmp, start, size*sizeof(T));
			}
			else
			{
				for(size_t i=0; i<size;++i)
				{
					tmp[i] = start[i];
				}
			}
			delete start;
			start = tmp;
			end = start+size;
			end_of_storage = start+capacity;
		}
	}

protected:
	/*T* _data;
	size_t _size;
	size_t _capacity;*/
	Iterator start;
	Iterator end;
	Iterator end_of_storage;
	
};

template<class T>
void PrintVector(const Vector<T>& v)
{
	Vector<T>::ConstIterator it;
	it = v.Begin();
	while(it != v.End())
	{
		cout << *it <<" ";
		++it;
	}
	cout << endl;
}
