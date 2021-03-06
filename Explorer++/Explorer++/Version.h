#define MAJOR_VERSION	1
#define MINOR_VERSION	3
#define MICRO_VERSION	99
#define BUILD_VERSION	528

#define QUOTE_(x)		#x
#define QUOTE(x)		QUOTE_(x)

#define VERSION_STRING	QUOTE(MAJOR_VERSION.MINOR_VERSION.MICRO_VERSION.BUILD_VERSION)
#define VERSION_STRING_W	_T(QUOTE(MAJOR_VERSION.MINOR_VERSION.MICRO_VERSION.BUILD_VERSION))

#define BUILD_DATE		27/04/2013 23:09:20
#define BUILD_DATE_STRING	_T(QUOTE(BUILD_DATE))
