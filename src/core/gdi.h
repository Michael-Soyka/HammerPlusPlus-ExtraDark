//=========== Copyright 2026 Michael Soyka, All rights reserved. ==============//
//
// Purpose: Owners of GDI and theme handles, each one frees its handle at the end of the scope
//
//=============================================================================//

#pragma once

#include <windows.h>
#include <uxtheme.h>

// Bitmap, brush, pen or font, deleted with DeleteObject
template <typename Handle_t>
struct GdiObject_t
{
	explicit GdiObject_t( Handle_t hObject )
		: m_hObject( hObject )
	{
	}

	~GdiObject_t()
	{
		if ( m_hObject )
		{
			DeleteObject( m_hObject );
		}
	}

	GdiObject_t( const GdiObject_t & ) = delete;
	GdiObject_t &operator=( const GdiObject_t & ) = delete;

	Handle_t	m_hObject;
};

// An object selected into a DC until the end of the scope, a null one is not selected
struct SelectedObject_t
{
	SelectedObject_t( HDC hDC, HGDIOBJ hObject )
		: m_hDC( hDC )
	{
		if ( hObject )
		{
			m_hOld = SelectObject( hDC, hObject );
		}
	}

	~SelectedObject_t()
	{
		if ( m_hOld )
		{
			SelectObject( m_hDC, m_hOld );
		}
	}

	SelectedObject_t( const SelectedObject_t & ) = delete;
	SelectedObject_t &operator=( const SelectedObject_t & ) = delete;

	HDC		m_hDC;
	HGDIOBJ	m_hOld = nullptr;
};

// The whole DC state back at the end of the scope
struct SavedDC_t
{
	explicit SavedDC_t( HDC hDC )
		: m_hDC( hDC ), m_nSaved( SaveDC( hDC ) )
	{
	}

	~SavedDC_t()
	{
		RestoreDC( m_hDC, m_nSaved );
	}

	SavedDC_t( const SavedDC_t & ) = delete;
	SavedDC_t &operator=( const SavedDC_t & ) = delete;

	HDC	m_hDC;
	int	m_nSaved;
};

// CreateCompatibleDC
struct MemoryDC_t
{
	explicit MemoryDC_t( HDC hCompatible )
		: m_hDC( CreateCompatibleDC( hCompatible ) )
	{
	}

	~MemoryDC_t()
	{
		if ( m_hDC )
		{
			DeleteDC( m_hDC );
		}
	}

	MemoryDC_t( const MemoryDC_t & ) = delete;
	MemoryDC_t &operator=( const MemoryDC_t & ) = delete;

	HDC	m_hDC;
};

// GetDC or GetWindowDC of a window, released with ReleaseDC
struct WindowDC_t
{
	WindowDC_t( HWND hWnd, HDC hDC )
		: m_hWnd( hWnd ), m_hDC( hDC )
	{
	}

	~WindowDC_t()
	{
		if ( m_hDC )
		{
			ReleaseDC( m_hWnd, m_hDC );
		}
	}

	WindowDC_t( const WindowDC_t & ) = delete;
	WindowDC_t &operator=( const WindowDC_t & ) = delete;

	HWND	m_hWnd;
	HDC		m_hDC;
};

// BeginPaint and EndPaint
struct PaintDC_t
{
	explicit PaintDC_t( HWND hWnd )
		: m_hWnd( hWnd ), m_hDC( BeginPaint( hWnd, &m_Paint ) )
	{
	}

	~PaintDC_t()
	{
		EndPaint( m_hWnd, &m_Paint );
	}

	PaintDC_t( const PaintDC_t & ) = delete;
	PaintDC_t &operator=( const PaintDC_t & ) = delete;

	HWND		m_hWnd;
	PAINTSTRUCT	m_Paint = {};
	HDC			m_hDC;
};

// OpenThemeData, null when the window has no theme for the classes
struct ThemeData_t
{
	ThemeData_t( HWND hWnd, const wchar_t *pwszClasses )
		: m_hTheme( OpenThemeData( hWnd, pwszClasses ) )
	{
	}

	~ThemeData_t()
	{
		if ( m_hTheme )
		{
			CloseThemeData( m_hTheme );
		}
	}

	ThemeData_t( const ThemeData_t & ) = delete;
	ThemeData_t &operator=( const ThemeData_t & ) = delete;

	HTHEME	m_hTheme;
};
