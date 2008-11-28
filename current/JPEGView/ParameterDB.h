#pragma once

#include "ProcessParams.h"

#pragma pack(push)

#pragma pack(1)
class CParameterDBEntry {
public:
	friend class CParameterDB;

	CParameterDBEntry();

	// Gets or sets the 64 bit hash value of the entry
	__int64 GetHash() const { return hash; }
	void SetHash(__int64 h) { hash = h; }

	// Initialize the DB entry given image processing parameter, processing flags and rotation
	void InitFromProcessParams(const CImageProcessingParams & processParams, EProcessingFlags eFlags, int nRotation);

	// Write DB entry values to image processing parameters, processing flags and rotation
	void WriteToProcessParams(CImageProcessingParams & processParams, EProcessingFlags & eFlags, int & nRotation) const;

	// Initialize section of image to be displayed
	void InitGeometricParams(CSize sourceSize, double fZoom, CPoint offsets, CSize targetSize);

	// Write DB entry values to given parameters, using the given target size for the image
	void WriteToGeometricParams(double & fZoom, CPoint & offsets, CSize sourceSize, CSize targetSize);

	// Copies the color corrections from the DB entry to the given array of floats
	void GetColorCorrectionAmounts(float corrections[6]) const;

	// Gets if zoom and offset values are stored with this entry
	bool HasZoomOffsetStored() const;

private:
	__int64 hash;
	uint8 contrast;
	uint8 brightness;
	uint8 sharpen;
	uint8 lightenShadows;
	uint8 darkenHighlights;
	uint8 lightenShadowSteepness;
	uint8 colorCorrection;
	uint8 contrastCorrection;
	uint8 flags;
	uint8 colorCorrectionAmount[6]; // this equals the INI setting at creation time of DB entry
	int16 zoomRectLeft;
	int16 zoomRectTop;
	int16 zoomRectRigth;
	int16 zoomRectBottom;
	uint8 cyanRed;
	uint8 magentaGreen;
	uint8 yellowBlue;
	uint8 reserved[6]; // reserved for future use

	uint8 Convert(float value, float lowerLimit, float upperLimit, bool isLog10) const;
	float Convert(uint8 value, float lowerLimit, float upperLimit, bool isLog10) const;
};

#pragma pack(1)
struct ParameterDBHeader {
	uint32 nZero1;
	uint32 nZero2;
	uint32 nMagic1;
	uint32 nMagic2;
	uint32 nVersion;
	uint32 nFill[5];
};

#pragma pack(pop)

// Parameter database class, holding image processing parameters for images identified by a 64 bit
// pixel hash value
class CParameterDB {
public:
	// Singleton instance
	static CParameterDB& This();
	
	// Finds the entry with the given hash value in the DB and returns a pointer to the entry.
	// If NULL, no such entry exists
	CParameterDBEntry* FindEntry(__int64 nHash);

	// Deletes the entry with the given hash from the DB.
	// Returns if the entry was deleted or not. Note that when the entry does not exist, false is returned.
	bool DeleteEntry(__int64 nHash);

	// Adds the given entry to the DB. If an entry with the same hash already exists, it is replaced.
	// Returns if the entry was added or not.
	bool AddEntry(const CParameterDBEntry& newEntry);

	// Returns if the parameter DB is empty (no persistent param DB exists)
	bool IsEmpty() { return m_blockList.size() == 0; }

private:
	CParameterDB(void);
	~CParameterDB(void);

	struct DBBlock {
		CParameterDBEntry* Block;
		int BlockLen;
		int UsedEntries;
	};

	CRITICAL_SECTION m_csDBLock;

	static CParameterDB* sm_instance;
	std::list<DBBlock*> m_blockList;
	__int64 m_LRUHash;
	CParameterDBEntry* m_pLRUEntry;

	CParameterDBEntry* FindEntryInternal(__int64 nHash, int& nIndex);
	CParameterDBEntry* AllocateNewEntry(int& nIndex);
	bool LoadFromFile();
	bool SaveToFile(int nIndex, const CParameterDBEntry & dbEntry);
	bool ConvertVersion1To2(HANDLE hFile, const CString& sFileName);
};
