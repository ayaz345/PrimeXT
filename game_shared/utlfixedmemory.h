//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
// A growable memory class.
//===========================================================================//

#ifndef UTLFIXEDMEMORY_H
#define UTLFIXEDMEMORY_H

#ifdef _WIN32
#pragma once
#endif

#if MIMALLOC_ALLOCATOR
#include <mimalloc-override.h>
#endif

#pragma warning (disable:4100)
#pragma warning (disable:4514)

//-----------------------------------------------------------------------------
// The CUtlFixedMemory class:
// A growable memory class that allocates non-sequential blocks, but is indexed sequentially
//-----------------------------------------------------------------------------
template< class T >
class CUtlFixedMemory
{
public:
	// constructor, destructor
	CUtlFixedMemory( int nGrowSize = 0, int nInitSize = 0 );
	~CUtlFixedMemory();

	// Set the size by which the memory grows
	void Init( int nGrowSize = 0, int nInitSize = 0 );

	// here to match CUtlMemory, but only used by ResetDbgInfo, so it can just return NULL
	T* Base() { return NULL; }
	const T* Base() const { return NULL; }

protected:
	struct BlockHeader_t;

public:
	class Iterator_t
	{
	public:
		Iterator_t( BlockHeader_t *p, int i ) : m_pBlockHeader( p ), m_nIndex( i ) {}
		BlockHeader_t *m_pBlockHeader;
		int m_nIndex;

		bool operator==( const Iterator_t it ) const	{ return m_pBlockHeader == it.m_pBlockHeader && m_nIndex == it.m_nIndex; }
		bool operator!=( const Iterator_t it ) const	{ return m_pBlockHeader != it.m_pBlockHeader || m_nIndex != it.m_nIndex; }
	};
	Iterator_t First() const							{ return m_pBlocks ? Iterator_t( m_pBlocks, 0 ) : InvalidIterator(); }
	Iterator_t Next( const Iterator_t &it ) const
	{
		assert( IsValidIterator( it ) );
		if ( !IsValidIterator( it ) )
			return InvalidIterator();

		BlockHeader_t * RESTRICT pHeader = it.m_pBlockHeader;
		if ( it.m_nIndex + 1 < pHeader->m_nBlockSize )
			return Iterator_t( pHeader, it.m_nIndex + 1 );

		return pHeader->m_pNext ? Iterator_t( pHeader->m_pNext, 0 ) : InvalidIterator();
	}
	int GetIndex( const Iterator_t &it ) const
	{
		assert( IsValidIterator( it ) );
		if ( !IsValidIterator( it ) )
			return InvalidIndex();

		return ( int )( HeaderToBlock( it.m_pBlockHeader ) + it.m_nIndex );
	}
	bool IsIdxAfter( int i, const Iterator_t &it ) const
	{
		assert( IsValidIterator( it ) );
		if ( !IsValidIterator( it ) )
			return false;

		if ( IsInBlock( i, it.m_pBlockHeader ) )
			return i > GetIndex( it );

		for ( BlockHeader_t * RESTRICT  pbh = it.m_pBlockHeader->m_pNext; pbh; pbh = pbh->m_pNext )
		{
			if ( IsInBlock( i, pbh ) )
				return true;
		}
		return false;
	}
	bool IsValidIterator( const Iterator_t &it ) const	{ return it.m_pBlockHeader && it.m_nIndex >= 0 && it.m_nIndex < it.m_pBlockHeader->m_nBlockSize; }
	Iterator_t InvalidIterator() const					{ return Iterator_t( NULL, -1 ); }

	// element access
	T& operator[]( int i );
	const T& operator[]( int i ) const;
	T& Element( int i );
	const T& Element( int i ) const;

	// Can we use this index?
	bool IsIdxValid( int i ) const;
	static int InvalidIndex() { return 0; }

	// Size
	int NumAllocated() const;
	int Count() const { return NumAllocated(); }

	// Grows memory by max(num,growsize), and returns the allocation index/ptr
	void Grow( int num = 1 );

	// Makes sure we've got at least this much memory
	void EnsureCapacity( int num );

	// Memory deallocation
	void Purge();

protected:
	// Fast swap - WARNING: Swap invalidates all ptr-based indices!!!
	void Swap( CUtlFixedMemory< T > &mem );

	bool IsInBlock( int i, BlockHeader_t *pBlockHeader ) const
	{
		T *p = ( T* )i;
		const T *p0 = HeaderToBlock( pBlockHeader );
		return p >= p0 && p < p0 + pBlockHeader->m_nBlockSize;
	}

	struct BlockHeader_t
	{
		BlockHeader_t *m_pNext;
		int m_nBlockSize;
	};

	const T *HeaderToBlock( const BlockHeader_t *pHeader ) const { return ( T* )( pHeader + 1 ); }
	const BlockHeader_t *BlockToHeader( const T *pBlock ) const { return ( BlockHeader_t* )( pBlock ) - 1; }

	BlockHeader_t* m_pBlocks;
	int m_nAllocationCount;
	int m_nGrowSize;
};

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------

template< class T >
CUtlFixedMemory<T>::CUtlFixedMemory( int nGrowSize, int nInitAllocationCount )
: m_pBlocks( 0 ), m_nAllocationCount( 0 ), m_nGrowSize( 0 )
{
	Init( nGrowSize, nInitAllocationCount );
}

template< class T >
CUtlFixedMemory<T>::~CUtlFixedMemory()
{
	Purge();
}


//-----------------------------------------------------------------------------
// Fast swap - WARNING: Swap invalidates all ptr-based indices!!!
//-----------------------------------------------------------------------------
template< class T >
void CUtlFixedMemory<T>::Swap( CUtlFixedMemory< T > &mem )
{
	SWAP( m_pBlocks, mem.m_pBlocks );
	SWAP( m_nAllocationCount, mem.m_nAllocationCount );
	SWAP( m_nGrowSize, mem.m_nGrowSize );
}


//-----------------------------------------------------------------------------
// Set the size by which the memory grows - round up to the next power of 2
//-----------------------------------------------------------------------------
template< class T >
void CUtlFixedMemory<T>::Init( int nGrowSize /* = 0 */, int nInitSize /* = 0 */ )
{
	Purge();

	if ( nGrowSize == 0)
	{
		// Compute an allocation which is at least as big as a cache line...
		nGrowSize = ( 31 + sizeof( T ) ) / sizeof( T );
	}
	m_nGrowSize = nGrowSize;

	Grow( nInitSize );
}

//-----------------------------------------------------------------------------
// element access
//-----------------------------------------------------------------------------
template< class T >
inline T& CUtlFixedMemory<T>::operator[]( int i )
{
	assert( IsIdxValid(i) );
	return *( T* )i;
}

template< class T >
inline const T& CUtlFixedMemory<T>::operator[]( int i ) const
{
	assert( IsIdxValid(i) );
	return *( T* )i;
}

template< class T >
inline T& CUtlFixedMemory<T>::Element( int i )
{
	assert( IsIdxValid(i) );
	return *( T* )i;
}

template< class T >
inline const T& CUtlFixedMemory<T>::Element( int i ) const
{
	assert( IsIdxValid(i) );
	return *( T* )i;
}


//-----------------------------------------------------------------------------
// Size
//-----------------------------------------------------------------------------
template< class T >
inline int CUtlFixedMemory<T>::NumAllocated() const
{
	return m_nAllocationCount;
}


//-----------------------------------------------------------------------------
// Is element index valid?
//-----------------------------------------------------------------------------
template< class T >
inline bool CUtlFixedMemory<T>::IsIdxValid( int i ) const
{
#ifdef _DEBUG
	for ( BlockHeader_t *pbh = m_pBlocks; pbh; pbh = pbh->m_pNext )
	{
		if ( IsInBlock( i, pbh ) )
			return true;
	}
	return false;
#else
	return i != InvalidIndex();
#endif
}

template< class T >
void CUtlFixedMemory<T>::Grow( int num )
{
	if ( num <= 0 )
		return;

	int nBlockSize = m_nGrowSize;
	if ( nBlockSize == 0 )
	{
		if ( m_nAllocationCount )
		{
			nBlockSize = m_nAllocationCount;
		}
		else
		{
			// Compute an allocation which is at least as big as a cache line...
			nBlockSize = ( 31 + sizeof( T ) ) / sizeof( T );
			assert( nBlockSize );
		}
	}
	if ( nBlockSize < num )
	{
		int n = ( num + nBlockSize -1 ) / nBlockSize;
		assert( n * nBlockSize >= num );
		assert( ( n - 1 ) * nBlockSize < num );
		nBlockSize *= n;
	}
	m_nAllocationCount += nBlockSize;

	BlockHeader_t *  RESTRICT pBlockHeader = ( BlockHeader_t* )malloc( sizeof( BlockHeader_t ) + nBlockSize * sizeof( T ) );
	if ( !pBlockHeader )
	{
		this->Error( "CUtlFixedMemory overflow!\n" );
	}
	pBlockHeader->m_pNext = NULL;
	pBlockHeader->m_nBlockSize = nBlockSize;

	if ( !m_pBlocks )
	{
		m_pBlocks = pBlockHeader;
	}
	else
	{
#if 1		// IsIdxAfter assumes that newly allocated blocks are at the end
		BlockHeader_t *  RESTRICT  pbh = m_pBlocks;
		while ( pbh->m_pNext )
		{
			pbh = pbh->m_pNext;
		}
		pbh->m_pNext = pBlockHeader;
#else
		pBlockHeader = m_pBlocks;
		pBlockHeader->m_pNext = m_pBlocks;
#endif
	}
}


//-----------------------------------------------------------------------------
// Makes sure we've got at least this much memory
//-----------------------------------------------------------------------------
template< class T >
inline void CUtlFixedMemory<T>::EnsureCapacity( int num )
{
	Grow( num - NumAllocated() );
}


//-----------------------------------------------------------------------------
// Memory deallocation
//-----------------------------------------------------------------------------
template< class T >
void CUtlFixedMemory<T>::Purge()
{
	if ( !m_pBlocks )
		return;

	for ( BlockHeader_t *pbh = m_pBlocks; pbh; )
	{
		BlockHeader_t *pFree = pbh;
		pbh = pbh->m_pNext;
		free( pFree );
	}
	m_pBlocks = NULL;
	m_nAllocationCount = 0;
}

#endif // UTLFIXEDMEMORY_H

