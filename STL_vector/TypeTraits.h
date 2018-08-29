struct TrueType
{
	bool Get()
	{
		return true;
	}
};
struct FalseType
{
	bool Get()
	{
		return false;
	}
};

template <class T>
struct TypeTraits
{
	typedef FalseType IsPODType; //Ç¶Ì×ÀàÐÍÉùÃ÷
};

template <>
struct TypeTraits<bool>
{
	typedef TrueType IsPODType; 
};

template<>
struct TypeTraits<char>
{
	typedef TrueType IsPODType;
};

template <>
struct TypeTraits<unsigned char>
{
	typedef TrueType IsPODType;
};

template<>
struct TypeTraits<short>
{
	typedef TrueType IsPODType;
}; 
template <>
struct TypeTraits<unsigned short>
{
	typedef TrueType IsPODType;
};

template<>
struct TypeTraits<int>
{
	typedef TrueType IsPODType;
}; 
template <>
struct TypeTraits<unsigned int>
{
	typedef TrueType IsPODType;
};

template<>
struct TypeTraits<long>
{
	typedef TrueType IsPODType;
}; 
template <>
struct TypeTraits<unsigned long>
{
	typedef TrueType IsPODType;
};

template<>
struct TypeTraits<long long>
{
	typedef TrueType IsPODType;
}; 
template <>
struct TypeTraits<unsigned long long>
{
	typedef TrueType IsPODType;
};

template<>
struct TypeTraits<float>
{
	typedef TrueType IsPODType;
}; 
template <>
struct TypeTraits<double>
{
	typedef TrueType IsPODType;
};

template<>
struct TypeTraits<long double>
{
	typedef TrueType IsPODType;
}; 
template <class T>
struct TypeTraits<T*>
{
	typedef TrueType IsPODType;
};

