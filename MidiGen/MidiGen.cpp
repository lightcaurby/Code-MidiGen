// MidiGen.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "pch.h"
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <cctype>
#include <vector>
#include <set>
#include <map>
#include <string>
#include <boost/algorithm/string.hpp>
#include <AtlBase.h>
#include <atlconv.h>


#include "..\mididata\MIDIData.h"

static std::map< std::string, std::string > s_mapOptions( {
	{ std::make_pair( "TITLE", "" ) },
	{ std::make_pair( "TICKS_PER_QUARTER", "120" ) },
	{ std::make_pair( "TEMPO_BPM", "70" ) },
	{ std::make_pair( "TIME_SIGNATURE_NUMERATOR", "4" ) },
	{ std::make_pair( "TIME_SIGNATURE_DENOMINATOR", "4" ) },
	{ std::make_pair( "SOURCE_TIME_SIGNATURE_NUMERATOR", "0" ) },
	{ std::make_pair( "SOURCE_TIME_SIGNATURE_DENOMINATOR", "0" ) },
	{ std::make_pair( "TRANSPOSE", "0" ) },
	{ std::make_pair( "MELODY_CHANNEL", "1" ) },
	{ std::make_pair( "MELODY_PATCH", "0 0 0" ) },
	{ std::make_pair( "CHORD_CHANNEL", "16" ) },
	{ std::make_pair( "CHORD_SYSEX_NUDGE", "0" ) },
	{ std::make_pair( "CHORD_NOTES_NUDGE", "0" ) },
	{ std::make_pair( "SECTION_ENDING1_NUDGE", "-1" ) },
	{ std::make_pair( "SECTION_NUDGE", "0" ) },
} );

const std::string& GetOption(
	std::string szOption
)
{
	const std::string& szValue = s_mapOptions[ szOption ];
	return szValue;
}

long GetOptionAsLong(
	std::string szOption
)
{
	const std::string& szValue = s_mapOptions[ szOption ];
	return atol( szValue.c_str() );
}

void SetOptionAsLong(
	std::string szOption,
	long lValue
)
{
	char szBuffer[ sizeof( long ) * 8 + 1 ];
	_ltoa( lValue, szBuffer, 10 );
	s_mapOptions[ szOption ] = szBuffer;
}

static std::set< long > s_setValidBaseLengths( {
	1, 2, 4, 8, 16, 32, 64
} );

static std::vector< std::string > s_vecKeySymbols({
	"C",
	"C#",
	"D",
	"D#",
	"E",
	"F",
	"F#",
	"G",
	"G#",
	"A",
	"A#",
	"B"
	} );

static std::map< char, int > s_mapKeysBySymbol( {
	{ std::make_pair( 'C', 0 ) },
	{ std::make_pair( 'D', 2 ) },
	{ std::make_pair( 'E', 4 ) },
	{ std::make_pair( 'F', 5 ) },
	{ std::make_pair( 'G', 7 ) },
	{ std::make_pair( 'A', 9 ) },
	{ std::make_pair( 'B', 11 ) },
} );

/*!
Helper macros for 64-to-32 integer conversions. Macros call conversion function with
souce file and line information.
*/
#ifdef _WIN64

#define X64_UINT_TO_32( x ) ( UInt64To32( x, __FILE__, __LINE__ ) )
#define X64_INT_TO_32( x ) ( Int64To32( x, __FILE__, __LINE__ ) )

#else

#define X64_UINT_TO_32( x ) ( UINT )( x ) 
#define X64_INT_TO_32( x ) ( INT )( x )

#endif // _WIN64

/*!
Exception to be thrown if overflow error occurs in X64-related integer conversion.
The error handling framework will catch this exception.
*/
class CX64OverflowException
{
	// Public interface.
public:

	//! Constructor.
	CX64OverflowException(
		IN const char* pszFile,  //!< File name for error.
		IN int iLine  //!< Line number for error.
	) :
		m_pszFile( pszFile ),
		m_iLine( iLine )
	{
	}

	//! Accesses the file name for error.
	const char* AccessFile() const
	{
		return m_pszFile;
	}

	//! Gets the line number for error.
	int GetLine() const
	{
		return m_iLine;
	}

	// Private interface.
private:

	//! Default constructor, unimplemented.
	CX64OverflowException();

	// Private data.
private:

	const char* m_pszFile;  //!< File name for error.
	int m_iLine;  //!< Line number for error.
};

/*!
Converts unsigned 64-bit integer to unsigned 32-bit integer.
Throws CMX64OverflowException on overflow.
*/
unsigned __int32 UInt64To32(
	IN unsigned __int64 ui64,  //!< Integer to convert.
	IN const char* strFile,  //!< Source file for conversion.
	IN int iLine  //!< Source line for conversion.
)
{
	// Convert the number.
	unsigned __int32 ui = static_cast< unsigned __int32 >( ui64 );

	// Check for overflow and throw and exception on error.
	if(static_cast< unsigned __int64 >( ui ) != ui64)
		throw CX64OverflowException( strFile, iLine );

	// Return the result.
	return ui;
}

/*!
Converts signed 64-bit integer to signed 32-bit integer.
Throws CMX64OverflowException on overflow.
*/
__int32 Int64To32(
	IN __int64 i64,  //!< Integer to convert.
	IN const char* strFile,  //!< Source file for conversion.
	IN int iLine  //!< Source line for conversion.
)
{
	// Convert the number.
	__int32 i = static_cast< __int32 >( i64 );

	// Check for overflow and throw and exception on error.
	if(static_cast< __int64 >( i ) != i64)
		throw CX64OverflowException( strFile, iLine );

	// Return the result.
	return i;
}

long PowerOfTwo( unsigned long lNumber )
{
	if( lNumber == 0 )
		return -1;

	for( unsigned long ulPower = 1; ulPower < 5; ulPower++)
	{
		// This for loop used shifting for powers of 2, meaning
		// that the value will become 0 after the last shift
		// (from binary 1000...0000 to 0000...0000) then, the 'for'
		// loop will break out.

		unsigned long ulPowerResult = lround(pow(2, ulPower));

		if( ulPowerResult == lNumber )
			return ulPower;
		if( ulPowerResult > lNumber )
			return -1;
	}
	return -1;
}

class CPosition
{
public:
	CPosition() :
		m_lBar( -1 ),
		m_lBeat( -1 ),
		m_lTick( -1 )
	{
	}

	virtual  ~CPosition()
	{
	}

	void Set(
		long lBar,
		long lBeat,
		long lTick
	)
	{
		m_lBar = lBar;
		m_lBeat = lBeat;
		m_lTick = lTick;
	}


	long GetBar() const
	{
		return m_lBar;
	}

	long GetBeat() const
	{
		return m_lBeat;
	}

	long GetTick() const
	{
		return m_lTick;
	}

	bool IsValid() const
	{
		return ( m_lBar > 0 && m_lBeat > 0 && m_lTick >= 0 );
	}

	void ShiftTicks(
		long lTicks
	)
	{
		long lTickPerQuarter = GetOptionAsLong( "TICKS_PER_QUARTER" );

		m_lTick += lTicks;
		while(m_lTick >= lTickPerQuarter)
		{
			ShiftBeats( 1 );
			m_lTick -= lTickPerQuarter;
		}
		while(m_lTick < 0)
		{
			ShiftBeats( -1 );
			m_lTick += lTickPerQuarter;
		}
	}

	void ShiftBeats(
		long lBeats
	)
	{
		long lTimeSignatureNumerator = GetOptionAsLong( "TIME_SIGNATURE_NUMERATOR" );

		m_lBeat += lBeats;
		while(m_lBeat >= lTimeSignatureNumerator)
		{
			ShiftBars( 1 );
			m_lBeat -= lTimeSignatureNumerator;
		}
		while(m_lBeat < 1)
		{
			ShiftBars( -1 );
			m_lBeat += lTimeSignatureNumerator;
		}
	}

	void ShiftBars(
		long lBars
	)
	{
		m_lBar += lBars;
		if(m_lBar < 1)
			m_lBar = 0;
	}

	bool operator==( const CPosition& x ) const
	{
		return m_lBar == x.GetBar() &&
			m_lBeat == x.GetBeat() &&
			m_lTick == x.GetTick();
	}

	bool operator!=( const CPosition& x ) const
	{
		return ( *this == x ) == false;
	}

	bool operator<( const CPosition& x ) const
	{
		return m_lBar < x.GetBar() ||
			( m_lBar == x.GetBar() && m_lBeat < x.GetBeat() ) ||
			( m_lBar == x.GetBar() && m_lBeat == x.GetBeat() && m_lTick < x.GetTick() );
	}

	bool operator>( const CPosition& x ) const
	{
		return !operator<( x ) && !operator==( x );
	}

private:
	long m_lBar;
	long m_lBeat;
	long m_lTick;

};

std::map< CPosition, long > s_mapModulations;

class CEvent
{
public:

	CEvent(
		const char* pszPos = nullptr,
		bool bPossiblyRelativePos = false
	) :
		m_pos(),
		m_bNoExplicitPosition( true )
	{
		Init( pszPos, bPossiblyRelativePos );
	}

	virtual ~CEvent()
	{
	}

	void Init(
		const char* pszPos,
		bool bPossiblyRelativePos
	)
	{
		// Parse the note position.
		ParsePosition( pszPos, bPossiblyRelativePos );
	}

	const CPosition& AccessPosition() const
	{
		return m_pos;
	}

	long GetBar() const
	{
		return m_pos.GetBar();
	}

	long GetBeat() const
	{
		return m_pos.GetBeat();
	}

	long GetTick() const
	{
		return m_pos.GetTick();
	}

	void ShiftTicks(
		long lTicks
	)
	{
		m_pos.ShiftTicks( lTicks );
	}

	bool HasNoExplicitPosition() const
	{
		return ( m_bNoExplicitPosition );
	}

	bool HasExplicitPosition() const
	{
		return ( !HasNoExplicitPosition() );
	}

	bool HasValidPosition() const
	{
		return m_pos.IsValid();
	}

	void SetPosition(
		long lBar,
		long lBeat,
		long lTick
	)
	{
		m_pos.Set( lBar, lBeat, lTick );
		m_bNoExplicitPosition = false;
	}

	virtual bool IsPause() const
	{
		return false;
	}

	virtual long GetDurationTicks() const
	{
		return 0;
	}

	virtual bool IsLayered() const 
	{
		return false;
	}

	virtual bool IsValid() const
	{
		return false;
	}

	static void ResetBarTimeline(
		long* plNewBarCBN
	)
	{
		s_lBarTimeline = plNewBarCBN ? *plNewBarCBN : 0;
	}

	static long DeltaBarTimeline(
		long lDeltaBar
	)
	{
		s_lBarTimeline += lDeltaBar;
		return s_lBarTimeline;
	}

protected:

	void ParsePosition(
		const char* pszPos,
		bool bPossiblyRelativePos
	)
	{
		if(pszPos)
		{
			// Locals.
			long lBar = 0;
			long lBeat = 0;
			long lTick = 0;

			// Check for relative bar.
			long lBarPossiblyRelative = 0;
			if( pszPos[0] == '/' )
			{
				// Relative bar.
				sscanf( &pszPos[ 1 ], "%lu:%lu:%lu",
					&lBarPossiblyRelative,
					&lBeat,
					&lTick );
				lBar = CEvent::DeltaBarTimeline( lBarPossiblyRelative );
				if( bPossiblyRelativePos == false )
					CEvent::DeltaBarTimeline( -lBarPossiblyRelative );
			}
			else
			{
				// Absolute bar.
				sscanf( pszPos, "%lu:%lu:%lu",
					&lBarPossiblyRelative,
					&lBeat,
					&lTick );
				lBar = lBarPossiblyRelative;
				if( bPossiblyRelativePos )
					CEvent::ResetBarTimeline( &lBar );
			}

			// Set the position.
			m_pos.Set( lBar, lBeat, lTick );
			m_bNoExplicitPosition = ( m_pos.IsValid() == false );
		}
		else
		{
			m_bNoExplicitPosition = true;
		}
	}

private:
	bool m_bNoExplicitPosition;
	CPosition m_pos;

	static long s_lBarTimeline;

};

long CEvent::s_lBarTimeline = 0;

// tuple: { data, paramcount }
static std::map< std::string, std::tuple< std::vector< unsigned char >, long > > s_mapSysexByName( {

	{ { "Accompaniment::Start" },		{ { 0xF0, 0x43, 0x60, 0x7A, 0xF7 }, 0 } },
	{ { "Accompaniment::Stop" },		{ { 0xF0, 0x43, 0x60, 0x7D, 0xF7 }, 0 } },

	{ { "SectionControl::Intro1" },		{ { 0xF0, 0x43, 0x7E, 0x00, 0x00, 0x7F, 0xF7 }, 0 } },
	{ { "SectionControl::Intro2" },		{ { 0xF0, 0x43, 0x7E, 0x00, 0x01, 0x7F, 0xF7 }, 0 } },
	{ { "SectionControl::Intro3" },		{ { 0xF0, 0x43, 0x7E, 0x00, 0x02, 0x7F, 0xF7 }, 0 } },
	{ { "SectionControl::MainA" },		{ { 0xF0, 0x43, 0x7E, 0x00, 0x08, 0x7F, 0xF7 }, 0 } },
	{ { "SectionControl::MainB" },		{ { 0xF0, 0x43, 0x7E, 0x00, 0x09, 0x7F, 0xF7 }, 0 } },
	{ { "SectionControl::MainC" },		{ { 0xF0, 0x43, 0x7E, 0x00, 0x0A, 0x7F, 0xF7 }, 0 } },
	{ { "SectionControl::MainD" },		{ { 0xF0, 0x43, 0x7E, 0x00, 0x0B, 0x7F, 0xF7 }, 0 } },
	{ { "SectionControl::FillA" },		{ { 0xF0, 0x43, 0x7E, 0x00, 0x10, 0x7F, 0xF7 }, 0 } },
	{ { "SectionControl::FillB" },		{ { 0xF0, 0x43, 0x7E, 0x00, 0x11, 0x7F, 0xF7 }, 0 } },
	{ { "SectionControl::FillC" },		{ { 0xF0, 0x43, 0x7E, 0x00, 0x12, 0x7F, 0xF7 }, 0 } },
	{ { "SectionControl::FillD" },		{ { 0xF0, 0x43, 0x7E, 0x00, 0x13, 0x7F, 0xF7 }, 0 } },
	{ { "SectionControl::Break" },		{ { 0xF0, 0x43, 0x7E, 0x00, 0x18, 0x7F, 0xF7 }, 0 } },
	{ { "SectionControl::Ending1" },	{ { 0xF0, 0x43, 0x7E, 0x00, 0x20, 0x7F, 0xF7 }, 0 } },
	{ { "SectionControl::Ending2" },	{ { 0xF0, 0x43, 0x7E, 0x00, 0x21, 0x7F, 0xF7 }, 0 } },
	{ { "SectionControl::Ending3" },	{ { 0xF0, 0x43, 0x7E, 0x00, 0x22, 0x7F, 0xF7 }, 0 } },

	{ { "SectionControl::Intro1Off" }, { { 0xF0, 0x43, 0x7E, 0x00, 0x00, 0x00, 0xF7 }, 0 } },
	{ { "SectionControl::Intro2Off" }, { { 0xF0, 0x43, 0x7E, 0x00, 0x01, 0x00, 0xF7 }, 0 } },
	{ { "SectionControl::Intro3Off" }, { { 0xF0, 0x43, 0x7E, 0x00, 0x02, 0x00, 0xF7 }, 0 } },
	{ { "SectionControl::MainAOff" }, { { 0xF0, 0x43, 0x7E, 0x00, 0x08, 0x00, 0xF7 }, 0 } },
	{ { "SectionControl::MainBOff" }, { { 0xF0, 0x43, 0x7E, 0x00, 0x09, 0x00, 0xF7 }, 0 } },
	{ { "SectionControl::MainCOff" }, { { 0xF0, 0x43, 0x7E, 0x00, 0x0A, 0x00, 0xF7 }, 0 } },
	{ { "SectionControl::MainDOff" }, { { 0xF0, 0x43, 0x7E, 0x00, 0x0B, 0x00, 0xF7 }, 0 } },
	{ { "SectionControl::FillAOff" }, { { 0xF0, 0x43, 0x7E, 0x00, 0x10, 0x00, 0xF7 }, 0 } },
	{ { "SectionControl::FillBOff" }, { { 0xF0, 0x43, 0x7E, 0x00, 0x11, 0x00, 0xF7 }, 0 } },
	{ { "SectionControl::FillCOff" }, { { 0xF0, 0x43, 0x7E, 0x00, 0x12, 0x00, 0xF7 }, 0 } },
	{ { "SectionControl::FillDOff" }, { { 0xF0, 0x43, 0x7E, 0x00, 0x13, 0x00, 0xF7 }, 0 } },
	{ { "SectionControl::BreakOff" }, { { 0xF0, 0x43, 0x7E, 0x00, 0x18, 0x00, 0xF7 }, 0 } },
	{ { "SectionControl::Ending1Off" }, { { 0xF0, 0x43, 0x7E, 0x00, 0x20, 0x00, 0xF7 }, 0 } },
	{ { "SectionControl::Ending2Off" }, { { 0xF0, 0x43, 0x7E, 0x00, 0x21, 0x00, 0xF7 }, 0 } },
	{ { "SectionControl::Ending3Off" }, { { 0xF0, 0x43, 0x7E, 0x00, 0x22, 0x00, 0xF7 }, 0 } },

	{ { "Vocals::HarmonyOn" }, { { 0xF0, 0x43, 0x10, 0x4C, 0x04, 0x00, 0x0C, 0x40, 0xF7 }, 0 } },
	{ { "Vocals::HarmonyOff" }, { { 0xF0, 0x43, 0x10, 0x4C, 0x04, 0x00, 0x0C, 0x7F, 0xF7 }, 0 } },
	{ { "Vocals::EffectOn" }, { { 0xF0, 0x43, 0x10, 0x4C, 0x04, 0x00, 0x76, 0x7F, 0xF7 }, 0 } },
	{ { "Vocals::EffectOff" }, { { 0xF0, 0x43, 0x10, 0x4C, 0x04, 0x00, 0x76, 0x00, 0xF7 }, 0 } },

	} );

class CSysex : public CEvent
{
public:
	CSysex(
		const char *pszPos = nullptr
	) :
		CEvent( pszPos, true )
	{
	}

	CSysex(
		const char *pszPos,
		const char *pszName
	) :
		CEvent( pszPos, true )
	{
		Init( pszPos, pszName, std::vector< unsigned char >() );
	}

	CSysex(
		const char *pszPos,
		const std::vector< unsigned char >& vecData
	) :
		CEvent( pszPos, true )
	{
		Init( pszPos, nullptr, vecData );
	}

	void Init(
		const char *pszPos,
		const char *pszName,
		const std::vector< unsigned char >& vecData
	)
	{
		if(pszName)
			ParseSysex( pszName );
		else
			m_vecData = vecData;

		if(pszName)
		{
			if( strstr( pszName, "SectionControl::Ending1" ) == pszName )
			{
				ShiftTicks( GetOptionAsLong( "SECTION_ENDING1_NUDGE" ) );
			}
			else if(strstr( pszName, "SectionControl::" ) == pszName)
			{
				CPosition posStart;
				posStart.Set( 1, 1, 0 );
				if( posStart != AccessPosition() )
					ShiftTicks( GetOptionAsLong( "SECTION_NUDGE" ) );
			}
		}

		// Valid?
		if(IsValid() == false)
		{	
			printf( "Invalid sysex: %s %s\n", pszPos, pszName );
		}
	}

	virtual bool IsValid() const override
	{
		bool bHasValidPosition = HasNoExplicitPosition() || HasValidPosition();
		bool bIsValidSysex = ( bHasValidPosition &&
			HasValidData() );

		return bIsValidSysex;
	}

	bool HasValidData() const
	{
		return ( m_vecData.size() > 2 && m_vecData.front() == 0xF0 && m_vecData.back() == 0xF7 );
	}

	const std::vector< unsigned char >& AccessData() const
	{
		return m_vecData;
	}

protected:

	void ParseSysex(
		const char *pszName
	)
	{
		auto itr = s_mapSysexByName.find( pszName );
		if(itr != s_mapSysexByName.end())
			m_vecData = std::get< 0 >( itr->second );
	}

private:
	std::vector< unsigned char > m_vecData;
};

class CNote : public CEvent
{
	static const long s_lMaxInputLen = 4;
	static const long s_lVelocity = 100;

public:

	enum class BindingMode { None, Layered, Slurred };
	enum class ValidationMode { Normal, Chord };

	CNote(
		const char* pszPos = nullptr
	) :
		CEvent( pszPos, false ),
		m_lOctave( -1 ),
		m_lKeyBase( -1 ),
		m_lVelocity( s_lVelocity ),
		m_lDurationTicks( -1 ),
		m_bindingMode ( BindingMode::None ),
		m_validationMode( ValidationMode::Normal ),
		m_bPause( false )
	{
	}

	CNote(
		const char *pszPosCBN,
		const char* pszKey,
		const char *pszLength,
		BindingMode bindingMode = BindingMode::None,
		ValidationMode validationMode = ValidationMode::Normal
	) :
	CNote( pszPosCBN )
	{
		m_bindingMode = bindingMode;
		m_validationMode = validationMode;
		Init( pszPosCBN, pszKey, pszLength );
	}


	virtual ~CNote()
	{
	}

	void Init(
		const char *pszPos,
		const char* pszKey,
		const char *pszLength
	)
	{
		// Parse the note representation.
		ParseNote( pszKey );

		// Parse the note length.
		ParseLength( pszLength );

		// Valid?
		if(IsValid() == false)
		{
			printf( "Invalid note: %s %s %s\n", pszPos, pszKey, pszLength );
		}
	}

	virtual bool IsValid() const override
	{
		bool bHasValidPosition = HasNoExplicitPosition() || HasValidPosition();
		bool bHasValidKey = ( HasValidKeyBase() && ( HasValidOctave() || IsValidatedNormally() == false ) );
		bool bIsValidPause = ( IsPause() &&	HasValidTicks() );
		bool bHasValidTicks = ( HasValidTicks() || IsValidatedNormally() == false );
		bool bIsValidNote = ( bHasValidPosition &&
				bHasValidKey &&
				bHasValidTicks &&
				HasValidVelocity() );

		return bIsValidPause || bIsValidNote;
	}

	long GetKey() const
	{
		return ( m_lOctave * 12 ) + m_lKeyBase;
	}

	long GetVel() const
	{
		return m_lVelocity;
	}

	virtual long GetDurationTicks() const override
	{
		return m_lDurationTicks;
	}

	virtual bool IsLayered() const override
	{
		return m_bindingMode == BindingMode::Layered;
	}

	bool IsSlurred() const
	{
		return m_bindingMode == BindingMode::Slurred;
	}

	bool IsValidatedNormally() const
	{
		return m_validationMode == ValidationMode::Normal;
	}

	virtual bool IsPause() const override
	{
		return m_bPause;
	}

	bool HasValidKeyBase() const
	{
		return ( m_lKeyBase >= 0 );
	}

	bool HasValidVelocity() const
	{
		return ( m_lVelocity > 0 );
	}

	bool HasValidTicks() const
	{
		return ( m_lDurationTicks > 0 );
	}

	bool HasValidOctave() const
	{
		return m_lOctave >= 0;
	}

	void SetOctave(
		long lOctave
	)
	{
		m_lOctave = lOctave;
	}

	void Transpose(
		long lInterval
	)
	{
		m_lKeyBase += lInterval;
		if(m_lKeyBase > 11)
		{
			m_lKeyBase -= 12;
			m_lOctave++;
		}
		else if(m_lKeyBase < 0)
		{
			m_lKeyBase += 12;
			m_lOctave--;
		}
	}

	void SetBindingMode(
		BindingMode bindingMode
	)
	{
		m_bindingMode = bindingMode;
	}

	void SetVelocity(
		long lVelocity
	)
	{
		m_lVelocity = lVelocity;
	}

	void SetDurationTicks(
		long lDurationTicks
	)
	{
		m_lDurationTicks = lDurationTicks;
	}

	void AdjustTimeSignature ()
	{
		long lTimeSigNumerator = GetOptionAsLong ( "TIME_SIGNATURE_NUMERATOR" );
		long lTimeSigDenominator = GetOptionAsLong ( "TIME_SIGNATURE_DENOMINATOR" );
		long lSourceTimeSigNumerator = GetOptionAsLong ( "SOURCE_TIME_SIGNATURE_NUMERATOR" );
		long lSourceTimeSigDenominator = GetOptionAsLong ( "SOURCE_TIME_SIGNATURE_DENOMINATOR" );
		if ( lSourceTimeSigNumerator > 0 &&
			lSourceTimeSigDenominator > 0 )
		{
			double dRatio = (( double )lTimeSigNumerator / ( double )lTimeSigDenominator) / ( ( double )lSourceTimeSigNumerator / ( double )lSourceTimeSigDenominator);
			m_lDurationTicks = ( long )( ( double )m_lDurationTicks * ( dRatio / 2 ) );
		}
	}

protected:
	long DetermineOctave(
		char szInput[],
		size_t stStartPos
	)
	{
		long lOctave = -1;

		_ASSERTE( stStartPos <= s_lMaxInputLen );
		if(szInput[ stStartPos ] >= 48 && szInput[ stStartPos ] <= 57)
		{
			lOctave = szInput[ stStartPos ] - 48;
		}
		else
		{
			// By default.
			lOctave = 5;

			// Count transposes.
			std::string s( &szInput[ stStartPos ] );
			size_t stPlus = std::count( s.begin(), s.end(), '+' );
			size_t stMinus = std::count( s.begin(), s.end(), '-' );
			lOctave += X64_UINT_TO_32( stPlus );
			lOctave -= X64_UINT_TO_32( stMinus );
		}

		_ASSERTE( lOctave >= 0 );
		return lOctave;

	}

	void ParseNote(
		const char* pszKey
	)
	{
		// Store the note input.
		memset( m_szInput, 0, s_lMaxInputLen + 1 );
		for(size_t st = 0; st < s_lMaxInputLen; st++)
			m_szInput[ st ] = ( pszKey && strlen( pszKey ) > st ) ? pszKey[ st ] : ' ';

		if(m_szInput[ 0 ] == '-')
		{
			m_bPause = true;
		}
		else
		{
			auto itr = s_mapKeysBySymbol.find( m_szInput[ 0 ] );
			if(itr != s_mapKeysBySymbol.end())
			{
				m_lKeyBase = itr->second;
			}
			if(m_szInput[ 1 ] == '#')
			{
				m_lKeyBase++;
				if(m_lKeyBase > 11)
					m_lKeyBase -= 12;
				m_lOctave = DetermineOctave( m_szInput, 2 );
			}
			else if(m_szInput[ 1 ] == 'b')
			{
				m_lKeyBase--;
				if(m_lKeyBase < 0)
					m_lKeyBase += 12;
				m_lOctave = DetermineOctave( m_szInput, 2 );
			}
			else
			{
				m_lOctave = DetermineOctave( m_szInput, 1 );
			}

			// Global transpose.
			Transpose( GetOptionAsLong( "TRANSPOSE" ) );

			// Modulation.
			if(AccessPosition().GetBar() > 0)
			{
				long lModulationHere = 0;
				std::map< CPosition, long >::const_iterator itrMod = s_mapModulations.begin();
				while(itrMod != s_mapModulations.end())
				{
					if(itrMod->first > AccessPosition())
						break;
					lModulationHere = itrMod->second;
					itrMod++;
				}
				if(lModulationHere != 0)
					Transpose( lModulationHere );
			}

		}
	}

	void ParseLength(
		const char* pszLength
	)
	{
		if(pszLength)
		{
			long lBaseLength = -1;
			sscanf( pszLength, "%lu", &lBaseLength );
			if(s_setValidBaseLengths.find( lBaseLength ) != s_setValidBaseLengths.end())
			{
				// Detect adjustments.
				std::string s( pszLength );
				size_t stPlus = std::count( s.begin(), s.end(), '+' );
				size_t stMinus = std::count( s.begin(), s.end(), '-' );

				// Calculate tick count.
				CalculateTicks( lBaseLength, stPlus, stMinus );
			}
		}
	}

	void CalculateTicks(
		long lBaseLength,
		size_t stPlus,
		size_t stMinus
	)
	{
		long lBaseDurationTicks = ( 4 * GetOptionAsLong( "TICKS_PER_QUARTER" ) ) / lBaseLength;
		long lDurationTicks = lBaseDurationTicks;
		long lAdditionalPercentage = 0;

		// Dot adjustment.
		switch(stPlus)
		{
		case 1:
			lAdditionalPercentage = 50;
			lDurationTicks = lBaseDurationTicks + ( ( lAdditionalPercentage * lBaseDurationTicks ) / 100 );
			break;
		case 2:
			lAdditionalPercentage = 75;
			lDurationTicks = lBaseDurationTicks + ( ( lAdditionalPercentage * lBaseDurationTicks ) / 100 );
			break;
		default:
			break;
		}

		// Triplet adjustment.
		switch(stMinus)
		{
		case 1:
			lDurationTicks = ( 2 * lDurationTicks ) / 3;
			break;
		default:
			break;
		}

		// Store the value.
		m_lDurationTicks = lDurationTicks;
	}

private:
	char m_szInput[ s_lMaxInputLen + 1 ];
	long m_lOctave;
	long m_lKeyBase;
	long m_lVelocity;
	long m_lDurationTicks;
	BindingMode m_bindingMode;
	ValidationMode m_validationMode;
	bool m_bPause;
};

static CNote s_noteChordLowest = CNote( nullptr, "C3", "1" );
static CNote s_noteChordHighest = CNote( nullptr, "F#4", "1" );

static std::map< std::string, unsigned char > s_mapChordRootsBySymbol( {
	{ { "C" },	{ 0x31 } },
	{ { "C#" }, { 0x41 } },
	{ { "Db" }, { 0x22 } },
	{ { "D" },	{ 0x32 } },
	{ { "D#" }, { 0x42 } },
	{ { "Eb" }, { 0x23 } },
	{ { "E" },	{ 0x33 } },
	{ { "E#" }, { 0x43 } },
	{ { "Fb" }, { 0x24 } },
	{ { "F" },	{ 0x34 } },
	{ { "F#" }, { 0x44 } },
	{ { "Gb" }, { 0x25 } },
	{ { "G" },	{ 0x35 } },
	{ { "G#" }, { 0x45 } },
	{ { "Ab" }, { 0x26 } },
	{ { "A" },	{ 0x36 } },
	{ { "A#" }, { 0x46 } },
	{ { "Bb" }, { 0x27 } },
	{ { "B" },	{ 0x37 } },
	{ { "B#" }, { 0x47 } },
	{ { "Cb" }, { 0x21 } },
	} );


static std::map< std::string, unsigned char > s_mapChordTypesBySymbol( {
	{ { "1+8" },	{ 0x1E } },
	{ { "1+5" },	{ 0x1F } },
	{ { "" },		{ 0x00 } },
	{ { "6" },		{ 0x01 } },
	{ { "M7" },		{ 0x02 } },
	{ { "M7b5" },	{ 0x15 } },
	{ { "M7#11" },	{ 0x03 } },
	{ { "+9" },		{ 0x04 } },
	{ { "M7+9" },	{ 0x05 } },
	{ { "6+9" },	{ 0x06 } },
	//{ { "b5" },		{  } },
	{ { "aug" },	{ 0x07 } },
	{ { "7aug" },	{ 0x1D } },
	{ { "M7aug" },	{ 0x1C } },
	{ { "m" },		{ 0x08 } },
	{ { "m6" },		{ 0x09 } },
	{ { "m7" },		{ 0x0A } },
	{ { "m7b5" },	{ 0x0B } },
	{ { "m+9" },	{ 0x0C } },
	{ { "m7+9" },	{ 0x0D } },
	{ { "m7+11" },	{ 0x0E } },
	//{ { "mM7b5" },	{  } },
	{ { "mM7" },	{ 0x0F } },
	{ { "mM7+9" },	{ 0x10 } },
	{ { "dim" },	{ 0x11 } },
	{ { "dim7" },	{ 0x12 } },
	{ { "7" },		{ 0x13 } },
	{ { "7sus4" },	{ 0x14 } },
	{ { "7+9" },	{ 0x16 } },
	{ { "7#11" },	{ 0x17 } },
	{ { "7+13" },	{ 0x18 } },
	{ { "7b5" },	{ 0x15 } },
	{ { "7b9" },	{ 0x19 } },
	{ { "7b13" },	{ 0x1A } },
	{ { "7#9" },	{ 0x1B } },
	{ { "sus4" },	{ 0x20 } },
	{ { "sus2" },	{ 0x21 } },
	{ { "CANCEL" },	{ 0x22 } },
	} );


static std::map< std::string, std::vector< long > > s_mapChordsBySymbol( {
	{ { "1+8" },	{ 0, 12 } },
	{ { "1+5" },	{ 0, 7 } },
	{ { "" },		{ 0, 4, 7 } },
	{ { "6" },		{ 0, 4, 7, 9 } },
	{ { "M7" },		{ 0, 4, 7, 11 } },
	{ { "M7b5" },	{ 0, 4, 6, 11 } },
	{ { "M7#11" },	{ 0, 4, 6, 7, 11 } },
	{ { "+9" },		{ 0, 2, 4, 7 } },
	{ { "M7+9" },	{ 0, 2, 4, 11 } },
	{ { "6+9" },	{ 0, 2, 4, 9 } },
	//{ { "b5" },		{ 0, 4, 6 } },
	{ { "aug" },	{ 0, 4, 8 } },
	{ { "7aug" },	{ 0, 4, 8, 10 } },
	{ { "M7aug" },	{ 0, 8, 11 } },
	{ { "m" },		{ 0, 3, 7 } },
	{ { "m6" },		{ 0, 3, 7, 9 } },
	{ { "m7" },		{ 0, 3, 10 } },
	{ { "m7b5" },	{ 0, 3, 6, 10 } },
	{ { "m+9" },	{ 0, 2, 3, 7 } },
	{ { "m7+9" },	{ 0, 2, 3, 10 } },
	{ { "m7+11" },	{ 0, 3, 5, 7 } },
	//{ { "mM7b5" },	{ 0, 3, 6, 11 } },
	{ { "mM7" },	{ 0, 3, 11 } },
	{ { "mM7+9" },	{ 0, 2, 3, 11 } },
	{ { "dim" },	{ 0, 3, 6 } },
	{ { "dim7" },	{ 0, 3, 6, 9 } },
	{ { "7" },		{ 0, 4, 7, 10 } },
	{ { "7sus4" },	{ 0, 5, 7, 10 } },
	{ { "7+9" },	{ 0, 2, 4, 10 } },
	{ { "7#11" },	{ 0, 4, 6, 7, 10 } },
	{ { "7+13" },	{ 0, 4, 9, 10 } },
	{ { "7b5" },	{ 0, 4, 6, 10 } },
	{ { "7b9" },	{ 0, 1, 4, 10 } },
	{ { "7b13" },	{ 0, 4, 7, 8, 10 } },
	{ { "7#9" },	{ 0, 3, 4, 10 } },
	{ { "sus4" },	{ 0, 5, 7 } },
	{ { "sus2" },	{ 0, 2, 7 } },
	{ { "CANCEL" }, { 0, 1, 2 } },
} );

class CChord
{
public:
	CChord()
	{
	}

	CChord(
		const char *pszPosCBN,
		const char* pszChordRoot,
		const char* pszChordType,
		const char* pszLength
	)
	{
		Init( pszChordRoot, pszChordType, nullptr, pszPosCBN, pszLength );
	}

	CChord(
		const char *pszPosCBN,
		const char* pszChordRoot,
		const char* pszChordType
	)
	{
		Init( pszChordRoot, pszChordType, nullptr, pszPosCBN, nullptr );
	}

	
	~CChord()
	{
	}

	void Init(
		const char* pszChordRoot,
		const char* pszChordType,
		const char* pszChordSlash,
		const char* pszPos,
		const char *pszLength
	)
	{
		ParseChordSymbol( pszChordRoot, pszChordType, pszChordSlash, pszPos, pszLength );
	}

	bool IsValid() const
	{
		return m_vecNotes.size() && m_sysex.IsValid();
	}

	const std::vector< CNote >& AccessNotes() const
	{
		return m_vecNotes;
	}

	const CSysex& AccessSysex() const
	{
		return m_sysex;
	}

	bool NotesHaveExplicitPosition() const
	{
		bool bRet = false;
		if(IsValid())
		{
			const CNote& note = m_vecNotes.front();
			bRet = note.HasExplicitPosition();
		}
		return bRet;
	}

private:

	long removeAll( std::string& str, const std::string& x ) {
		if( x.empty() )
			return 0;
		size_t start_pos = 0;
		std::string to( "" );
		long lCount = 0;
		while( ( start_pos = str.find( x, start_pos ) ) != std::string::npos ) {
			lCount++;
			str.replace( start_pos, x.length(), to );
			start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
		}
		return lCount;
	}

	void ParseChordSymbol(
		const char* pszChordRoot,
		const char* pszChordType,
		const char* pszChordSlash,
		const char* pszPos,
		const char *pszLength
	)
	{
		// First interpret the root for modulation.
		std::string szChordRoot( pszChordRoot ? pszChordRoot : "" );
		long lTransposeUp = removeAll( szChordRoot, ">" );
		long lTransposeDown = removeAll( szChordRoot, "<" );
		long lModulation = lTransposeUp - lTransposeDown;;

		// As notes.
		CNote noteRoot( pszPos, szChordRoot.c_str(), pszLength,
			CNote::BindingMode::None,
			CNote::ValidationMode::Chord );

		// Modulation if any.
		if(lModulation != 0)
			s_mapModulations[ noteRoot.AccessPosition() ] = lModulation;

		long lModulationHere = 0;
		std::map< CPosition, long >::const_iterator itr = s_mapModulations.begin();
		while(itr != s_mapModulations.end())
		{
			if(itr->first > noteRoot.AccessPosition())
				break;
			lModulationHere = itr->second;
			itr++;
		}
		if( lModulationHere != 0 )
			noteRoot.Transpose( lModulation );


		// Further note processing.
		if(noteRoot.HasValidOctave() == false)
			noteRoot.SetOctave( 3 );
		noteRoot.ShiftTicks( GetOptionAsLong( "CHORD_NOTES_NUDGE" ) );
		if(noteRoot.IsValidatedNormally() == false || noteRoot.IsValid())
		{
			m_vecNotes.push_back( noteRoot );
			auto itr = s_mapChordsBySymbol.find( pszChordType );
			if(itr != s_mapChordsBySymbol.end())
			{
				const std::vector< long >& vecIntervals = itr->second;
				for(auto lInterval : vecIntervals)
				{
					if(lInterval == 0)
						continue;

					CNote note = noteRoot;
					//***note.SetBindingMode( CNote::BindingMode::Layered );
					note.Transpose( lInterval );
					m_vecNotes.push_back( note );
				}
			}
		}
		else if(noteRoot.IsValidatedNormally() == false)
		{
			//printf( "Invalid note: %s %s %s\n", pszPos, szChordRoot.c_str(), pszLength );
		}

		// As SYSEX.
		unsigned char byteChordRoot = 0xFF;
		long lIndex = noteRoot.GetKey() % 12;
		const std::string& szChordRootTransposed = s_vecKeySymbols[ lIndex ];
		auto itr1 = s_mapChordRootsBySymbol.find( szChordRootTransposed );
		if(itr1 != s_mapChordRootsBySymbol.end())
			byteChordRoot = itr1->second;

		unsigned char byteChordType = 0xFF;
		auto itr2 = s_mapChordTypesBySymbol.find( pszChordType );
		if(itr2 != s_mapChordTypesBySymbol.end())
			byteChordType = itr2->second;

		std::vector< unsigned char > vecData;
		if(byteChordRoot != 0xFF && byteChordType != 0xFF)
		{
			vecData = {
				0xF0, 0x43, 0x7E, 0x02, byteChordRoot, byteChordType, 0x7F, 0x7F, 0xF7
			};
		}

		m_sysex = CSysex( pszPos, vecData );
		m_sysex.ShiftTicks( GetOptionAsLong( "CHORD_SYSEX_NUDGE" ) );
		if( m_sysex.IsValid() == false )
			printf( "Invalid chord sysex: %s %s %s\n", pszPos, szChordRoot.c_str(), pszChordType );
	}

private:
	std::vector< CNote > m_vecNotes;
	CSysex m_sysex;
};

class CTrack
{
public:
	CTrack(
		MIDITrack* pMIDITrack,
		long lChannel,
		bool bChordAsAccmp
	) :
	m_pMIDITrackMeta( pMIDITrack ),
	m_lChannel( lChannel ),
	m_lCurrentTime( 0 ),
	m_bChordAsAccmp( bChordAsAccmp )
	{
		m_pMIDITrack = m_pMIDITrackMeta->m_pNextTrack;
	}

	~CTrack()
	{
		m_pMIDITrackMeta = nullptr;
		m_pMIDITrack = nullptr;
	}

	long DetermineAbsoluteTime(
		const CEvent& event,
		const CEvent& eventPrevious
	)
	{
		long lTime = 0;
		if(event.HasExplicitPosition())
		{
			// Explicit position		
			MIDITrack_MakeTime( m_pMIDITrackMeta,
				event.GetBar() - 1,
				event.GetBeat() - 1,
				event.GetTick(),
				&lTime );
		}
		else
		{
			// Implicit position.
			lTime = m_lCurrentTime;
			
			// Layered position.
			if(event.IsLayered() && eventPrevious.IsValid())
				lTime -= eventPrevious.GetDurationTicks();
		}

		return lTime;
	}

	long Insert(
		CNote& note,
		bool bExplicitNoteOff,
		bool bModulateIfNeeded
	)
	{
		long lRet = 0;
		if(m_pMIDITrack && note.IsValid())
		{
			long lTime = DetermineAbsoluteTime( note, m_notePrevious );
			
			if(note.IsPause())
			{
				// no-op
				lRet = 1;
			}
			else
			{
				// Are we allowed to modulate?
				if ( bModulateIfNeeded )
				{
					long lModulationHere = 0;
					std::map< CPosition, long >::const_iterator itrMod = s_mapModulations.begin ();
					while ( itrMod != s_mapModulations.end () )
					{
						long lTimeForModulation = 0;
						MIDITrack_MakeTime ( m_pMIDITrackMeta,
							itrMod->first.GetBar () - 1,
							itrMod->first.GetBeat () - 1,
							itrMod->first.GetTick (),
							&lTimeForModulation );
						if ( lTimeForModulation > lTime )
							break;
						lModulationHere = itrMod->second;
						itrMod++;
					}
					if ( lModulationHere != 0 )
						note.Transpose ( lModulationHere );
				}

				bool bSlurred = false;
				if( note.IsSlurred() )
				{
					MIDIEvent* pEvent = MIDITrack_GetLastKindEvent( m_pMIDITrack, MIDIEVENT_NOTEON );
					if( pEvent &&
						pEvent->m_lLen == 3 &&
						pEvent->m_pData[ 2 ] == 0 &&
						pEvent->m_pData[ 1 ] == note.GetKey() )
					{
						pEvent->m_lTime += note.GetDurationTicks();
						bSlurred = true;
					}
				}

				if(bSlurred == false)
				{
					if(bExplicitNoteOff)
					{
						lRet = MIDITrack_InsertNoteOn( m_pMIDITrack, lTime,
							m_lChannel,
							note.GetKey(),
							note.GetVel() );

						lTime += note.GetDurationTicks() - 1;

						lRet = MIDITrack_InsertNoteOn( m_pMIDITrack, lTime,
							m_lChannel,
							note.GetKey(),
							0 );
					}
					else
					{
						lRet = MIDITrack_InsertNote( m_pMIDITrack, lTime,
							m_lChannel,
							note.GetKey(),
							note.GetVel(),
							note.GetDurationTicks() );
					}
				}
			}

			m_notePrevious = note.IsPause() ? CNote() : note;
			m_lCurrentTime = lTime + note.GetDurationTicks();
		}
		return lRet;
	}

	long Insert(
		const CSysex& sysex
	)
	{
		long lRet = 0;
		if(m_pMIDITrack && sysex.IsValid())
		{
			long lTime = DetermineAbsoluteTime( sysex, CEvent() );

			unsigned char szBuffer[ 256 ];
			memset( szBuffer, 0, 256 );
			memcpy( szBuffer, &sysex.AccessData().front(), sysex.AccessData().size() );
			lRet = MIDITrack_InsertSysExEvent( m_pMIDITrack, lTime,
				szBuffer,
				X64_UINT_TO_32( sysex.AccessData().size() ) );

			m_lCurrentTime = lTime;
		}
		return lRet;
	}

	long InsertDelayed(
		const std::vector< CNote >& vecNotes,
		const CPosition& pos,
		long lDuration
	)
	{
		for(auto& note : vecNotes)
		{
			if(note.IsValid() == false)
				continue;

			CNote noteToInsert = note;
			noteToInsert.SetPosition(
				pos.GetBar(),
				pos.GetBeat(),
				pos.GetTick() );
			noteToInsert.SetDurationTicks( lDuration );
			while(noteToInsert.GetKey() > s_noteChordHighest.GetKey())
				noteToInsert.Transpose( -12 );
			while(noteToInsert.GetKey() < s_noteChordLowest.GetKey())
				noteToInsert.Transpose( 12 );

			Insert( noteToInsert, true, false );
		}

		return 1;
	}

	long Insert(
		const CChord& chord,
		bool bAutoLength = false
	)
	{
		long lRet = 0;
		if(m_pMIDITrack && chord.IsValid())
		{
			if( bAutoLength )
			{
				const CNote& noteCurrentFirst = chord.AccessNotes().front();

				if(m_chordPrevious.IsValid())
				{
					const CNote& notePrevFirst = m_chordPrevious.AccessNotes().front();
					{
						// Note on.
						const std::vector< CNote >& vecNotes = m_chordPrevious.AccessNotes();

						long lTimePrev = 0;
						long lTimeCurrent = 0;
						MIDITrack_MakeTime( m_pMIDITrackMeta,
							notePrevFirst.GetBar() - 1,
							notePrevFirst.GetBeat() - 1,
							notePrevFirst.GetTick(),
							&lTimePrev );
						MIDITrack_MakeTime( m_pMIDITrackMeta,
							noteCurrentFirst.GetBar() - 1,
							noteCurrentFirst.GetBeat() - 1,
							noteCurrentFirst.GetTick(),
							&lTimeCurrent );
						long lDuration = lTimeCurrent - lTimePrev;

						InsertDelayed( vecNotes, notePrevFirst.AccessPosition(), lDuration );

					}

					lRet = 1;
				}

				m_chordPrevious = chord;

			}
			/***
			else if(chord.NotesHaveExplicitPosition())
			{
				// Note on.
				const std::vector< CNote >& vecNotes = chord.AccessNotes();
				for(const auto& note : vecNotes)
				{
					if(note.IsValid() == false)
						continue;

					CNote noteToInsert = note;
					if(ChordAsAccmp())
					{
						while(noteToInsert.GetKey() > s_noteChordHighest.GetKey())
							noteToInsert.Transpose( -12 );
						while(noteToInsert.GetKey() < s_noteChordLowest.GetKey())
							noteToInsert.Transpose( 12 );
					}

					Insert( noteToInsert, false );
				}

				lRet = 1;
			}
			***/
			else
			{
				const CSysex& sysex = chord.AccessSysex();
				Insert( sysex );
				lRet = 1;
			}
		}

		return lRet;
	}

	bool ChordAsAccmp() const
	{
		return m_bChordAsAccmp;
	}

	long GetCurrentTime() const
	{
		return m_lCurrentTime;
	}

	void Finalize(
		long lTrackEndTime = -1
	)
	{

		// check open chords
		if(m_chordPrevious.IsValid())
		{
			const CNote& notePrevFirst = m_chordPrevious.AccessNotes().front();
			{
				// Note on.
				const std::vector< CNote >& vecNotes = m_chordPrevious.AccessNotes();


				long lTimePrev = 0;
				MIDITrack_MakeTime( m_pMIDITrackMeta,
					notePrevFirst.GetBar() - 1,
					notePrevFirst.GetBeat() - 1,
					notePrevFirst.GetTick(),
					&lTimePrev );

				long lCurrentTime = ( lTrackEndTime != -1 ) ? lTrackEndTime : m_lCurrentTime;
				long lDuration = lCurrentTime - lTimePrev;

				InsertDelayed( vecNotes, notePrevFirst.AccessPosition(), lDuration );

			}
		}

		/* Insert end of track event */
		if( m_pMIDITrack )
			MIDITrack_InsertEndofTrack( m_pMIDITrack, GetTickCount() );
	}

	MIDITrack* AccessMetaTrack()
	{
		return m_pMIDITrackMeta;
	}

	MIDITrack* AccessTrack()
	{
		return m_pMIDITrack;
	}

private:
	MIDITrack* m_pMIDITrackMeta;
	MIDITrack* m_pMIDITrack;
	long m_lChannel;
	long m_lCurrentTime;
	bool m_bChordAsAccmp;
	CNote m_notePrevious;
	CChord m_chordPrevious;
};

class CLIParser {
public:
	CLIParser( int &argc, char **argv ) {
		for(int i = 1; i < argc; ++i)
			this->tokens.push_back( std::string( argv[ i ] ) );
	}
	/// @author iain
	const std::string& getCmdOption( const std::string &option ) const {
		std::vector<std::string>::const_iterator itr;
		itr = std::find( this->tokens.begin(), this->tokens.end(), option );
		if(itr != this->tokens.end() && ++itr != this->tokens.end()) {
			return *itr;
		}
		static const std::string empty_string( "" );
		return empty_string;
	}
	/// @author iain
	bool cmdOptionExists( const std::string &option ) const {
		return std::find( this->tokens.begin(), this->tokens.end(), option )
			!= this->tokens.end();
	}
private:
	std::vector <std::string> tokens;
};

// trim from start (in place)
static inline void ltrim( std::string &s ) {
	s.erase( s.begin(), std::find_if( s.begin(), s.end(), []( int ch ) {
		return !std::isspace( ch );
	} ) );
}

// trim from end (in place)
static inline void rtrim( std::string &s ) {
	s.erase( std::find_if( s.rbegin(), s.rend(), []( int ch ) {
		return !std::isspace( ch );
	} ).base(), s.end() );
}

// trim from both ends (in place)
static inline void trim( std::string &s ) {
	ltrim( s );
	rtrim( s );
}

int main( int argc, char **argv )
{

	int i = PowerOfTwo( 2 );
	i= PowerOfTwo( 4 );
	i= 	PowerOfTwo( 8 );

	std::vector< std::vector<std::string> > vecEvents;
	vecEvents.reserve( 1000 );

	const long lParts = 4;
	std::string szOutFileName[ lParts ];

	CLIParser input( argc, argv );
	std::string szInputFile = input.getCmdOption( "-i" );
	std::string szOutputDir = input.getCmdOption( "-o" );
	if(!szInputFile.empty() && !szOutputDir.empty())
	{
		FILE *infile;
		char szBuffer[ BUFSIZ ]; /* BUFSIZ is defined if you include stdio.h */

		infile = fopen( szInputFile.c_str(), "r" );
		if(!infile) 
		{
			printf( "\nfile '%s' not found\n", szInputFile.c_str() );
			return 0;
		}

		while(fgets( szBuffer, sizeof( szBuffer ), infile )) 
		{
			std::string szLine( szBuffer );
			trim( szLine );
			if(szLine.length() > 0 && szLine.front() != '#' )
			{
				std::vector<std::string> vecResults;
				boost::split( vecResults, szLine, []( char c ) {return c == '\t'; } );

				if(vecResults.size() >= 3)
				{
					if(vecResults.front() == "opt")
						s_mapOptions[ vecResults[ 1 ] ] = vecResults[ 2 ];
					else if(vecResults.front() == "syx" ||
						vecResults.front() == "vhx" ||
						vecResults.front() == "chd" ||
						vecResults.front() == "not")
						vecEvents.push_back( vecResults );
				}
			}
		}
	}

	boost::replace_all( szOutputDir, "\"", "" );
	szOutFileName[ 0 ] = szOutputDir + std::string( "\\" ) + GetOption( "TITLE" ) + std::string( "_accmp.mid" );
	szOutFileName[ 1 ] = szOutputDir + std::string( "\\" ) + GetOption( "TITLE" ) + std::string( "_chords.mid" );
	szOutFileName[ 2 ] = szOutputDir + std::string( "\\" ) + GetOption( "TITLE" ) + std::string( "_vh.mid" );
	szOutFileName[ 3 ] = szOutputDir + std::string( "\\" ) + GetOption( "TITLE" ) + std::string( "_melody.mid" );

	MIDIData* pMIDIData[ lParts ] = { nullptr };
	for(int i = 0; i < lParts; i++)
	{
		pMIDIData[ i ] = MIDIData_Create( MIDIDATA_FORMAT1, 2, MIDIDATA_TPQNBASE, GetOptionAsLong( "TICKS_PER_QUARTER" ) );
		if(pMIDIData[ i ])
		{
			long lTimeMax = 0;
			MIDITrack* pMIDITrack;
			pMIDITrack = pMIDIData[ i ]->m_pFirstTrack;
			MIDITrack_InsertTrackName( pMIDITrack, 0, L"Meta" ); /* Title */
			MIDITrack_InsertTempo( pMIDITrack, 0, 60000000 / GetOptionAsLong( "TEMPO_BPM" ) ); /* 120BPM */
			long lTimeSigNumerator = GetOptionAsLong( "TIME_SIGNATURE_NUMERATOR" );
			long lTimeSigDenominatorPowerOfTwo = PowerOfTwo( GetOptionAsLong( "TIME_SIGNATURE_DENOMINATOR" ) );
			if(lTimeSigDenominatorPowerOfTwo > -1)
				MIDITrack_InsertTimeSignature( pMIDITrack, 0, lTimeSigNumerator, lTimeSigDenominatorPowerOfTwo, 24, 8 ); /* 4/4 */
			MIDITrack_InsertEndofTrack( pMIDITrack, 0 );
		}
	}

	{
		/* Get pointer of the second track */
		MIDITrack* pMIDITrack0 = pMIDIData[ 0 ]->m_pFirstTrack;
		CTrack track0( pMIDITrack0, 1, true );

		MIDITrack* pMIDITrack1 = pMIDIData[ 1 ]->m_pFirstTrack;
		CTrack track1( pMIDITrack1, GetOptionAsLong( "CHORD_CHANNEL" ) - 1, true );
		MIDITrack_InsertControlChange( track1.AccessTrack(), 0, GetOptionAsLong( "CHORD_CHANNEL" ) - 1, 7, 0 );
		MIDITrack_InsertProgramChange( track1.AccessTrack(), 0, GetOptionAsLong( "CHORD_CHANNEL" ) - 1, 0 ); /* Piano1 */

		MIDITrack* pMIDITrack2 = pMIDIData[ 2 ]->m_pFirstTrack;
		CTrack track2( pMIDITrack2, 1, true );

		MIDITrack* pMIDITrack3 = pMIDIData[ 3 ]->m_pFirstTrack;
		CTrack track3( pMIDITrack3, GetOptionAsLong( "MELODY_CHANNEL" ) - 1, true );
		std::string szPatch = GetOption( "MELODY_PATCH" );
		long lMSB = 0;
		long lLSB = 0;
		long lPrg = 1;
		sscanf( szPatch.c_str(), "%ld %ld %ld", &lMSB, &lLSB, &lPrg );
		if(lMSB >= 0 && lLSB >= 0 && lPrg >= 0)
		{
			MIDITrack_InsertControlChange( track3.AccessTrack(), 0, GetOptionAsLong( "MELODY_CHANNEL" ) - 1, 0, lMSB );
			MIDITrack_InsertControlChange( track3.AccessTrack(), 0, GetOptionAsLong( "MELODY_CHANNEL" ) - 1, 32, lLSB );
			MIDITrack_InsertProgramChange( track3.AccessTrack(), 0, GetOptionAsLong( "MELODY_CHANNEL" ) - 1, std::max( 0L, lPrg - 1 ) ); /* Piano1 */
		}

		for(auto& vecEvent : vecEvents)
		{
			const std::string& szType = vecEvent[ 0 ];
			if(szType == "syx")
			{
				if(vecEvent.size() < 3)
					continue;
				CSysex sysex( vecEvent[ 1 ].c_str(), vecEvent[ 2 ].c_str() );
				track0.Insert( sysex );
			}
			else if(szType == "chd")
			{
				if(vecEvent.size() < 3 )
					continue;
				CChord chord( vecEvent[ 1 ].c_str(), vecEvent[ 2 ].c_str(),
					vecEvent.size() < 4 ? "" : vecEvent[ 3 ].c_str() );
				track0.Insert( chord );
				track1.Insert( chord, true );
			}
			else if(szType == "vhx")
			{
				if(vecEvent.size() < 3)
					continue;
				CSysex sysex( vecEvent[ 1 ].c_str(), vecEvent[ 2 ].c_str() );
				track2.Insert( sysex );
			}
			else if(szType == "not")
			{
				if(vecEvent.size() < 3)
					continue;

				CNote::BindingMode bm = CNote::BindingMode::None;
				if(vecEvent.size() > 3 && vecEvent[ 3 ] == "S")
					bm = CNote::BindingMode::Slurred;
				else if(vecEvent.size() > 3 && vecEvent[ 3 ] == "L")
					bm = CNote::BindingMode::Layered;
				CNote note( nullptr,
						vecEvent[ 1 ].c_str(),
						vecEvent[ 2 ].c_str(),
						bm );
				//note.AdjustTimeSignature();
				//printf ( "insert note: %ld %ld %ld %ld\n", note.GetBar(), note.GetBeat(), note.GetTick(), note.GetDurationTicks() );
				track3.Insert( note, false, true );
			}
			else
			{
				continue;
			}
		}

		track3.Finalize();
		track2.Finalize();
		track1.Finalize( track0.GetTickCount() );
		track0.Finalize();
	}

	/* Save MIDIData */
	for(int i = 0; i < lParts; i++)
	{
		if(pMIDIData[ i ] == NULL)
			continue;

		CA2W ca2w( szOutFileName[ i ].c_str() );
		std::wstring wszOutFileName = (LPWSTR) ca2w;
		MIDIData_SaveAsSMF( pMIDIData[ i ], wszOutFileName.c_str() );
	}
	
	/* Delete MIDIData */
	for(int i = 0; i < lParts; i++)
	{
		MIDIData_Delete( pMIDIData[ i ] );
		pMIDIData[ i ] = NULL;
	}
	
	return 1;
}

