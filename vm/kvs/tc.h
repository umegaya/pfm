#if !defined(__TC_H__)
#define __TC_H__

/* tokyocabinet */
class tc {
public:
	static int init(int) { return NBR_OK; }
	static void fin() { }
};

#endif

