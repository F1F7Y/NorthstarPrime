#include "tier0/utils.h"

std::error_code fsErrorCode;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
std::error_code FSGetLastErrorCode() noexcept
{
	return fsErrorCode;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool FileExists(fs::path path) noexcept
{
	std::error_code errorCode;
	return fs::exists(path, errorCode);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool IsDirectory(fs::path path) noexcept
{
	std::error_code errorCode;
	return fs::is_directory(path, errorCode);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CopyFile(fs::path from, fs::path to) noexcept
{
	std::error_code errorCode;
	fs::copy_file(from, to, errorCode);

	if (errorCode.value() == 0)
		return true;

	fsErrorCode = errorCode;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CreateDirectories(fs::path path) noexcept
{
	std::error_code errorCode;
	fs::create_directories(path, errorCode);

	if (errorCode.value() == 0)
		return true;

	fsErrorCode = errorCode;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool skip_valid_ansi_csi_sgr(char*& str)
{
	if (*str++ != '\x1B')
		return false;
	if (*str++ != '[') // CSI
		return false;
	for (char* c = str; *c; c++)
	{
		if (*c >= '0' && *c <= '9')
			continue;
		if (*c == ';' || *c == ':')
			continue;
		if (*c == 'm') // SGR
			break;
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void RemoveAsciiControlSequences(char* str, bool allow_color_codes)
{
	for (char *pc = str, c = *pc; c = *pc; pc++)
	{
		// skip UTF-8 characters
		int bytesToSkip = 0;
		if ((c & 0xE0) == 0xC0)
			bytesToSkip = 1; // skip 2-byte UTF-8 sequence
		else if ((c & 0xF0) == 0xE0)
			bytesToSkip = 2; // skip 3-byte UTF-8 sequence
		else if ((c & 0xF8) == 0xF0)
			bytesToSkip = 3; // skip 4-byte UTF-8 sequence
		else if ((c & 0xFC) == 0xF8)
			bytesToSkip = 4; // skip 5-byte UTF-8 sequence
		else if ((c & 0xFE) == 0xFC)
			bytesToSkip = 5; // skip 6-byte UTF-8 sequence

		bool invalid = false;
		char* orgpc = pc;
		for (int i = 0; i < bytesToSkip; i++)
		{
			char next = pc[1];

			// valid UTF-8 part
			if ((next & 0xC0) == 0x80)
			{
				pc++;
				continue;
			}

			// invalid UTF-8 part or encountered \0
			invalid = true;
			break;
		}
		if (invalid)
		{
			// erase the whole "UTF-8" sequence
			for (char* x = orgpc; x <= pc; x++)
				if (*x != '\0')
					*x = ' ';
				else
					break;
		}
		if (bytesToSkip > 0)
			continue; // this byte was already handled as UTF-8

		// an invalid control character or an UTF-8 part outside of UTF-8 sequence
		if ((iscntrl(c) && c != '\n' && c != '\r' && c != '\x1B') || (c & 0x80) != 0)
		{
			*pc = ' ';
			continue;
		}

		if (c == '\x1B') // separate handling for this escape sequence...
			if (allow_color_codes && skip_valid_ansi_csi_sgr(pc)) // ...which we allow for color codes...
				pc--;
			else // ...but remove it otherwise
				*pc = ' ';
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
std::string FormatAV(const char* fmt, va_list vArgs)
{
	va_list vArgsCopy;
	va_copy(vArgsCopy, vArgs);
	int iLen = std::vsnprintf(NULL, 0, fmt, vArgsCopy);
	va_end(vArgsCopy);

	std::string svResult;

	if (iLen > 0)
	{
		svResult.resize(iLen);
		std::vsnprintf(svResult.data(), iLen + 1, fmt, vArgs);
	}

	return svResult;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
std::string FormatA(const char* fmt, ...)
{
	std::string svResult;

	va_list vArgs;
	va_start(vArgs, fmt);
	svResult = FormatAV(fmt, vArgs);
	va_end(vArgs);

	return svResult;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
std::wstring FormatWV(const wchar_t* fmt, va_list vArgs)
{
	va_list vArgsCopy;
	va_copy(vArgsCopy, vArgs);
	int iLen = std::vswprintf(NULL, 0, fmt, vArgsCopy);
	va_end(vArgsCopy);

	std::wstring wsvResult;

	if (iLen > 0)
	{
		wsvResult.resize(iLen);
		std::vswprintf(reinterpret_cast<wchar_t*>(wsvResult.data()), iLen + 1, fmt, vArgs);
	}

	return wsvResult;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
std::wstring FormatW(const wchar_t* fmt, ...)
{
	std::wstring wsvResult;

	va_list vArgs;
	va_start(vArgs, fmt);
	wsvResult = FormatWV(fmt, vArgs);
	va_end(vArgs);

	return wsvResult;
}

std::string g_svTimeStamp;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
std::string CreateTimeStamp()
{
	if (!g_svTimeStamp.empty())
	{
		return g_svTimeStamp;
	}

	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

	std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

	time_t timer = std::chrono::system_clock::to_time_t(now);
	std::tm localtime = *std::localtime(&timer);

	std::ostringstream oss;
	oss << std::put_time(&localtime, "%Y-%m-%d_%H-%M-%S");
	oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

	g_svTimeStamp = oss.str();

	return g_svTimeStamp;
}
