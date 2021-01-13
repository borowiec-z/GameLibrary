#include "GameLibrary/Utilities/String.h"

#include <string>

#include "catch2/catch.hpp"

using namespace GameLibrary::Utilities;


TEST_CASE("Surround works with std::string, std::wstring, and their literals (no support for string surrounded by wstring yet).")
{
	SECTION("std::string")
	{
		const std::string s1 = "";
		const std::string s2 = "String";

		REQUIRE(surround(s1, "abc") == "abcabc");
		REQUIRE(surround(s2, "abc") == "abcStringabc");
		REQUIRE(surround(s2, '"') == "\"String\"");
		REQUIRE(surround("a", s2) == "StringaString");
		REQUIRE(surround("a", "b") == "bab");
	}

	SECTION("std::wstring")
	{
		const std::wstring w1 = L"";
		const std::wstring w2 = L"String";

		REQUIRE(surround(w1, L"abc") == L"abcabc");
		REQUIRE(surround(w2, L"abc") == L"abcStringabc");
		REQUIRE(surround(w2, L'"') == L"\"String\"");
		REQUIRE(surround(L"a", w2) == L"StringaString");
		REQUIRE(surround(L"a", L"b") == L"bab");
	}
}

TEST_CASE("Quote matches behavior of surround() with quotation mark argument.")
{
	REQUIRE(quote("str") == surround("str", '"'));
	REQUIRE(quote(L"str") == surround(L"str", L'"'));
	REQUIRE(quote("") == surround("", '"'));
	REQUIRE(quote(L"") == surround(L"", L'"'));
}

TEST_CASE("IsString() returns correct results.")
{
	REQUIRE(IsStringV<char*>);
	REQUIRE(IsStringV<const char*>);
	REQUIRE(IsStringV<std::string>);
	REQUIRE(IsStringV<const std::string&>);
	REQUIRE(IsStringV<std::string&&>);

	REQUIRE(IsStringV<wchar_t*>);
	REQUIRE(IsStringV<const wchar_t*>);
	REQUIRE(IsStringV<std::wstring>);
	REQUIRE(IsStringV<const std::wstring&>);
	REQUIRE(IsStringV<std::wstring&&>);

	REQUIRE_FALSE(IsStringV<int>);
	REQUIRE_FALSE(IsStringV<float&&>);
	REQUIRE_FALSE(IsStringV<struct Dummy>);
}
