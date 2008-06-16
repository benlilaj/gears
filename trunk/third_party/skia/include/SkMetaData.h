#ifndef SkMetaData_DEFINED
#define SkMetaData_DEFINED

#include "SkMath.h"

class SkMetaData {
public:
	SkMetaData();
	SkMetaData(const SkMetaData& src);
	~SkMetaData();

	SkMetaData&	operator=(const SkMetaData& src);

	void	reset();

	bool	findS32(const char name[], int32_t* value = nil) const;
	bool	findScalar(const char name[], SkScalar* value = nil) const;
	const SkScalar* findScalars(const char name[], int* count, SkScalar values[] = nil) const;
	const char*	findString(const char name[]) const;
	bool	findPtr(const char name[], void** value = nil) const;
	bool	findBool(const char name[], bool* value = nil) const;

	bool	hasS32(const char name[], int32_t value) const
	{
		int32_t	v;
		return this->findS32(name, &v) && v == value;
	}
	bool	hasScalar(const char name[], SkScalar value) const
	{
		SkScalar	v;
		return this->findScalar(name, &v) && v == value;
	}
	bool	hasString(const char name[], const char value[]) const
	{
		const char* v = this->findString(name);
		return	v == nil && value == nil ||
				v != nil && value != nil && !strcmp(v, value);
	}
	bool	hasPtr(const char name[], void* value) const
	{
		void*	v;
		return this->findPtr(name, &v) && v == value;
	}
	bool	hasBool(const char name[], bool value) const
	{
		bool	v;
		return this->findBool(name, &v) && v == value;
	}

	void	setS32(const char name[], int32_t value);
	void	setScalar(const char name[], SkScalar value);
	SkScalar* setScalars(const char name[], int count, const SkScalar values[] = nil);
	void	setString(const char name[], const char value[]);
	void	setPtr(const char name[], void* value);
	void	setBool(const char name[], bool value);

	bool	removeS32(const char name[]);
	bool	removeScalar(const char name[]);
	bool	removeString(const char name[]);
	bool	removePtr(const char name[]);
	bool	removeBool(const char name[]);

	SkDEBUGCODE(static void UnitTest();)

	enum Type {
		kS32_Type,
		kScalar_Type,
		kString_Type,
		kPtr_Type,
		kBool_Type,

		kTypeCount
	};

	struct Rec;
	class Iter;
	friend class Iter;

	class Iter {
	public:
		Iter() : fRec(nil) {}
		Iter(const SkMetaData&);

		/**	Reset the iterator, so that calling next() will return the first
			data element. This is done implicitly in the constructor.
		*/
		void	reset(const SkMetaData&);

		/**	Each time next is called, it returns the name of the next data element,
			or nil when there are no more elements. If non-nil is returned, then the
			element's type is returned (if not nil), and the number of data values
			is returned in count (if not nil).
		*/
		const char*	next(Type*, int* count);

	private:
		Rec* fRec;
	};

public:
	struct Rec {
		Rec*        fNext;
		uint16_t	fDataCount;	// number of elements
		uint8_t		fDataLen;	// sizeof a single element
#ifdef SK_DEBUG
		Type		fType;
#else
		uint8_t		fType;
#endif

#ifdef SK_DEBUG
		const char* fName;
		union {
			int32_t     fS32;
			SkScalar	fScalar;
			const char*	fString;
			void*		fPtr;
			bool		fBool;
		} fData;
#endif

		const void*	data() const { return (this + 1); }
		void*		data() { return (this + 1); }
		const char*	name() const { return (const char*)this->data() + fDataLen * fDataCount; }
		char*		name() { return (char*)this->data() + fDataLen * fDataCount; }

		static Rec* Alloc(size_t);
		static void Free(Rec*);
	};
	Rec*	fRec;

	const Rec* find(const char name[], Type) const;
	void* set(const char name[], const void* data, size_t len, Type, int count);
	bool remove(const char name[], Type);
};

#endif
