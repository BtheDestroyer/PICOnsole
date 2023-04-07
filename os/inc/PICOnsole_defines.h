#define KEEP __attribute__((__used__))
#define PICONSOLE_FUNC KEEP __attribute__ ((noinline))
#define PICONSOLE_MEMBER_FUNC PICONSOLE_FUNC virtual
#define GETTER [[nodiscard]]
