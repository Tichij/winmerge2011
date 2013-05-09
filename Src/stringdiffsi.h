/** 
 * @file  stringdiffsi.h
 *
 * @brief Declaration file for class stringdiffs
 *
 */
// ID line follows -- this is updated by SVN
// $Id$

#ifndef stringdiffsi_h_included
#define stringdiffsi_h_included

/**
 * @brief kind of diff blocks.
 */
enum sd_kind
{
	dlword,
	dlspace,
	dlbreak, 
	dlinsert,
};
/**
 * @brief kind of synchronaction
 */
enum sd_findsyn_func
{
	synbegin1,
	synbegin2,
	synend1, 
	synend2 
};

struct wdiff;

/**
 * @brief Class to hold together data needed to implement sd_ComputeWordDiffs
 */
class stringdiffs
{
public:
	stringdiffs(const String &str1, const String &str2,
		bool case_sensitive, int whitespace, int breakType,
		stl::vector<wdiff> &diffs);

	~stringdiffs();

	void BuildWordDiffList();
	void PopulateDiffs();

// Implementation types
private:
	struct word
	{
		int start; // index of first character of word in original string
		int end;   // index of last character of word in original string
		int hash;
		sd_kind kind; // Is it a isWordBreak 0 = word -1= whitespace -2 = empty 1 = breakWord (H�?)
		word(int s = 0, int e = 0, sd_kind k = dlword, int h = 0) : start(s), end(e), kind(k), hash(h) { }
		int length() const { return end + 1 - start; }
	};

// Implementation methods
private:

	void BuildWordsArray(const String &str, stl::vector<word> &words);
	static void InsertInWords(stl::vector<word> &words, int bw);
	int FindPreMatchInWords(const stl::vector<word> &words, const word &needword, int bw, int side) const;
	int FindNextMatchInWords(const stl::vector<word> &words, const word &needword, int bw, int side) const;
	static int FindNextSpaceInWords(const stl::vector<word> &words, int bw);
	static int FindPreNoInsertInWords(const stl::vector<word> &words, int bw);
	static int FindNextInsertInWords(const stl::vector<word> &words, int bw);
	static int FindNextNoInsertInWords(const stl::vector<word> &words, int bw);
	static void MoveInWordsUp(stl::vector<word> &words, int source, int target);
	static void MoveInWordsDown(stl::vector<word> &words, int source, int target);
	UINT Hash(const String &str, int begin, int end, UINT h) const;
	bool AreWordsSame(const word &word1, const word &word2) const;
	static bool IsWord(const word &);
	static bool IsSpace(const word &);
	static bool IsBreak(const word &);
	static bool IsInsert(const word &);
	bool caseMatch(TCHAR ch1, TCHAR ch2) const;

// Implementation data
private:
	const String &m_str1;
	const String &m_str2;
	bool m_case_sensitive;
	int m_whitespace;
	int m_breakType;
	bool m_matchblock;
	stl::vector<wdiff> &m_diffs;
	stl::vector<word> m_words1;
	stl::vector<word> m_words2;
	stl::vector<wdiff> m_wdiffs;
};


#endif // stringdiffsi_h_included
