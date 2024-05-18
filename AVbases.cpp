#include "Header.h"
template<typename T>
extern VOID WriteLog(const T& data, Tstring prefix);
AvBases::AvBases(const std::multimap<uint64_t, Record>& data)
	: records{ data }
{};

std::vector<Record> AvBases::FindRecord(uint64_t key)
{
	auto range = records.equal_range(key);
	std::vector<Record> records;
	for (auto it = range.first; it != range.second; ++it)
	{
		records.push_back(it->second);
	}
	return records;
}

std::shared_ptr<AvBases> AvBasesLoader::LoadBases(std::u16string path)
{
	std::multimap<uint64_t, Record>records;
	auto file = std::make_shared<FileStream>(path);
	BinarySerializer serializer(static_pointer_cast<IRandomAccessStream>(file));
	std::vector<uint8_t> data(9);
	std::vector<uint8_t> expectSig({ 'K','u','z','n','e','t','s','o','v' });
	if (!serializer.ReadBlob(data) || !(data == expectSig))
	{

		return nullptr;
	}
	uint64_t count;
	if (!serializer.ReadUInt64(count))
	{
		WriteLog("error count");
		return nullptr;
	}
	WriteLog("Count = ");
	WriteLog(count);
	for (int i = 0; i < count; i++)
	{
		std::vector<uint8_t> hash;
		Record record;
		if (!serializer.ReadUInt64(record.prefix))
		{
			WriteLog("error prefix");
			return nullptr;
		}
		WriteLog("prefix success");
		if (!serializer.ReadUInt32(record.len))
		{
			WriteLog("error len");
			return nullptr;
		}
		WriteLog("len success");
		record.hash.resize(32);
		if (!serializer.ReadBlob(record.hash))
		{
			WriteLog("error hash");
			return nullptr;
		}
		WriteLog("hash success");
		if (!serializer.ReadUInt32(record.offsetStart))
		{
			WriteLog("error offsetStart");
			return nullptr;
		}
		WriteLog("offStart success");
		if (!serializer.ReadUInt32(record.offsetEnd))
		{
			WriteLog("error offsetEnd");
			return nullptr;
		}
		WriteLog("offEnd success");
		if (!serializer.ReadString(record.name))
		{
			WriteLog("error name");
			return nullptr;
		}
		WriteLog("name success");
		records.insert(std::make_pair(record.prefix, record));
	}
	WriteLog("bases succesfully read!");
	return std::make_shared<AvBases>(records);
}
