/***************************************************************************
*============= Copyright by Darmstadt University of Applied Sciences =======
****************************************************************************
* Filename        : CJson.cpp
* Author          : Bharath Ramachandraiah
* Description     : The file defines all the methods pertaining to the
* 					class type - class CJson.
* 					The class CJson is implement the persistent feature using
* 					the CPersistanceStorage abstract class's interfaces.
*
****************************************************************************/

//System Include Files
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

//Own Include Files
#include "CJson.h"
#include "CPOI.h"

using namespace std;
using namespace APT;

CJson::CJson()
{
	// Do nothing
}

CJson::~CJson()
{
	// Do nothing
}

/**
* Set the name of the media to be used for persistent storage.
* The exact interpretation of the name depends on the implementation
* of the component.
*
* @param name the media to be used
* @returnval void
*/
void CJson::setMediaName(string name)
{
	this->mediaName = name;
}


/**
* Write the data to the persistent storage.
*
* @param waypointDb the data base with way points
* @param poiDb the database with points of interest
* @return true if the data could be saved successfully
*/
bool CJson::writeData (const CWpDatabase& waypointDb, const CPoiDatabase& poiDb)
{
	bool			ret = true;
	ofstream 		fileStream;
	string 			fileName;

	cout << "=======================================================\n";
	cout << "INFO: Waypoint Database backup request\n";

	fileName = this->mediaName;
	fileStream.precision(10);

	fileStream.open(fileName.c_str(), ofstream::out);

	if (!fileStream.fail())
	{
		Wp_Map_t		Waypoints;
		Poi_Map_t		Pois;
		CPOI::t_poi		type;
		string			name, description, typeName;
		double 			latitude, longitude;

		Waypoints = waypointDb.getWpsFromDatabase();

		// create waypoint object in the Json format
		fileStream << "{\n";
		fileStream << "\"waypoints\": [\n";

		for (Wp_Map_Itr_t itr = Waypoints.begin(); itr != Waypoints.end(); ++(itr))
		{
			// assuming all the elements in Database is valid
			itr->second.getAllDataByReference(name, latitude, longitude);

			fileStream << "\t{\n";
			fileStream << "\t\t\"name\": \"" << name << "\",\n";
			fileStream << "\t\t\"latitude\": " << latitude << ",\n";
			fileStream << "\t\t\"longitude\": " << longitude << "\n";

			// check if this is the last element in the database
			if (&(*itr) != &(*Waypoints.rbegin()))
			{
				fileStream << "\t},\n";
			}
			else
			{
				fileStream << "\t}\n";
			}

			if (fileStream.fail())
			{
				fileStream.clear();
				cout << "WARNING: Error writing a Waypoint into the file.\n" << itr->second << endl;
				ret = false;
			}
		}

		// end the waypoint object
		fileStream << "],\n";

		Pois = poiDb.getPoisFromDatabase();

		// create waypoint object in the Json format
		fileStream << "\"pois\": [\n";

		for (Poi_Map_Itr_t itr = Pois.begin(); itr != Pois.end(); ++(itr))
		{
			// assuming all the elements in Database is valid
			itr->second.getAllDataByReference(name, latitude, longitude, type, description);
			typeName = itr->second.getPoiTypeName();

			fileStream << "\t{\n";
			fileStream << "\t\t\"name\": \"" << name << "\",\n";
			fileStream << "\t\t\"latitude\": " << latitude << ",\n";
			fileStream << "\t\t\"longitude\": " << longitude << ",\n";
			fileStream << "\t\t\"type\": \"" << typeName << "\",\n";
			fileStream << "\t\t\"description\": \"" << description << "\"\n";

			if (&(*itr) != &(*Pois.rbegin()))
			{
				fileStream << "\t},\n";
			}
			else
			{
				fileStream << "\t}\n";
			}

			if (fileStream.fail())
			{
				fileStream.clear();
				cout << "WARNING: Error writing a POI into the file.\n" << itr->second << endl;
				ret = false;
			}
		}

		// end the poi object
		fileStream << "]\n}\n";
	}
	else
	{
		fileStream.clear();
		cout << "WARNING: Error opening the file to write - " << fileName << endl;
		ret = false;
	}

	fileStream.flush();
	fileStream.close();

	cout << "=======================================================\n";
	return ret;
}


/**
* Fill the databases with the data from persistent storage. If
* merge mode is MERGE, the content in the persistent storage
* will be merged with any content already existing in the data
* bases. If merge mode is REPLACE, already existing content
* will be removed before inserting the content from the persistent
* storage.
*
* @param waypointDb the the data base with way points
* @param poiDb the database with points of interest
* @param mode the merge mode
* @return true if the data could be read successfully
*/
bool CJson::readData (CWpDatabase& waypointDb, CPoiDatabase& poiDb, MergeMode mode)
{
	bool			ret = true;
	ifstream 		fileStream;
	string 			fileName;

	fileName = this->mediaName;
	fileStream.precision(10);
	fileStream.clear();

	// Read Waypoints
	fileStream.open(fileName.c_str(), ifstream::in);

	// is the open successful?
	if (!fileStream.fail())
	{
		cout << "=======================================================\n";
		if (mode == CJson::MERGE)
		{
			cout << "INFO: Waypoint Database Merge Request.\n";
		}
		else if (mode == CJson::REPLACE)
		{
			waypointDb.resetWpsDatabase();
			cout << "INFO: Waypoint Database Replace Request.\n";
		}
		else
		{
			cout << "ERROR: Waypoint Database Unknown MergeMode Request.\n";
			return false;
		}

		cout << "=======================================================\n";

		/*
		 * CJsonToken -> base class -> stores the type of a token
		 * CJsonValueToken -> derived template class(CJsonToken) -> stores the value associated with the token
		 * 												|-> CJsonStringToken
		 * 												|-> CJsonNumberToken
		 * 												|-> CJsonBoolToken
		 * yyFlexLexer -> base class -> parsing
		 * CJsonScanner -> derived class(yyFlexLexer) -> handles parsing and returns pointer to CJsonToken which points to CJsonValueToken(polymorphism)
		 */
		CJsonScanner 				scanner(fileStream);
		CJsonToken::TokenType 		event = CJsonToken::JSON_NULL, previousEvent = CJsonToken::JSON_NULL;
		CJson::readStates			currentState = WAITING_FOR_BEGIN_OBJECT, previousState = WAITING_FOR_BEGIN_OBJECT;
		CJsonStringToken 			*pTokenString = 0;
		CJson::jsonReadExceptions	error = CJson::JSON_ERR_NO;

		/*
		 * Exceptions:
		 * 1. Create
		 * 2. Report
		 * 3. Detect
		 * 4. Handle
		 * 5. Recover
		 */
		try
		{
			CPOI::t_poi		type = CPOI::DEFAULT_POI;
			string			name = "", description = "";
			double 			latitude = (LATITUDE_MAX + 1), longitude = (LONGITUDE_MAX  + 1);	// set to invalid values

			do
			{
				this->m_pToken = scanner.nextToken();

				if (this->m_pToken)
				{
					event = this->m_pToken->getType();

					switch (currentState)
					{
						case WAITING_FOR_BEGIN_OBJECT:
							if (event == CJsonToken::BEGIN_OBJECT)
							{
								previousState = currentState;
								currentState = WAITING_FOR_DB_NAME;
							}
							else
							{
								error = CJson::JSON_ERR_EXPECT_BEGIN_OBJECT;
							}
							break;

						case WAITING_FOR_DB_NAME:
							if (event == CJsonToken::STRING)
							{
								this->resetCurrentReadObjects();
								this->resetAllAttributesRead();

								pTokenString = dynamic_cast<CJsonStringToken *>(this->m_pToken);

								if (pTokenString && this->currentReadObject(pTokenString->getValue()))
								{
									previousState = currentState;
									currentState = WAITING_FOR_NAME_SEPARATOR;
								}
								else
								{
									error = CJson::JSON_ERR_EXPECT_DB_NAME_STRING;
								}
							}
							else
							{
								error = CJson::JSON_ERR_EXPECT_DB_NAME_STRING;
							}
							break;

						case WAITING_FOR_DB_ARRAY_BEGIN:
							if (event == CJsonToken::BEGIN_ARRAY)
							{
								previousState = currentState;
								currentState = WAITING_FOR_DB_OBJECT_BEGIN;
							}
							else
							{
								error = CJson::JSON_ERR_EXPECT_DB_ARRAY_BEGIN;
							}
							break;

						case WAITING_FOR_DB_OBJECT_BEGIN:
							if (event == CJsonToken::BEGIN_OBJECT)
							{
								this->resetAllAttributesRead();
								previousState = currentState;
								currentState = WAITING_FOR_ATTRIBUTE_NAME;
							}
							else
							{
								error = CJson::JSON_ERR_EXPECT_DB_OBJECT_BEGIN;
							}
							break;

						case WAITING_FOR_DB_OBJECT_END:
							if (event == CJsonToken::END_OBJECT)
							{
								previousState = currentState;
								currentState = WAITING_FOR_VALUE_SEPARATOR;
							}
							else
							{
								error = CJson::JSON_ERR_EXPECT_DB_OBJECT_END;
							}
							break;

						case WAITING_FOR_ATTRIBUTE_NAME:
							if (event == CJsonToken::STRING)
							{
								pTokenString = dynamic_cast<CJsonStringToken *>(this->m_pToken);

								if (pTokenString)
								{
									bool isValid = false;

									isValid = this->expectedAttributeValue(pTokenString->getValue());

									if (isValid)
									{
										previousState = currentState;
										currentState = WAITING_FOR_NAME_SEPARATOR;
									}
									else
									{
										error = CJson::JSON_ERR_EXPECT_ATTR_NAME;
									}
								}
							}
							break;

						case WAITING_FOR_NAME_SEPARATOR:
							if (event == CJsonToken::NAME_SEPARATOR)
							{
								if (previousState == WAITING_FOR_DB_NAME)
								{
									previousState = currentState;
									currentState = WAITING_FOR_DB_ARRAY_BEGIN;
								}
								else if (previousState == WAITING_FOR_ATTRIBUTE_NAME)
								{
									previousState = currentState;
									currentState = WAITING_FOR_VALUE;
								}
								else
								{
									error = CJson::JSON_ERR_EXPECT_NAME_SEPARATOR;
								}
							}
							else
							{
								error = CJson::JSON_ERR_EXPECT_NAME_SEPARATOR;
							}
							break;

						case WAITING_FOR_VALUE:
							if (event == this->m_exceptedTokenType && this->extractValue(name, latitude, longitude, type, description))
							{
								bool isAllRead = false;

								isAllRead = this->allAttributesRead();

								if (isAllRead)
								{
									if (this->m_currentObjectRead[WAYPOINTS])
									{
										CWaypoint wp(name, latitude, longitude);

										if (!wp.getName().empty())
										{
											waypointDb.addWaypoint(wp);
										}
										else
										{
											cout << "ERROR: Invalid Waypoint Values\n";
										}
									}
									else if (this->m_currentObjectRead[POINT_OF_INTEREST])
									{
										CPOI poi(type, name, description, latitude, longitude);

										if (!poi.getName().empty())
										{
											poiDb.addPoi(poi);
										}
										else
										{
											cout << "ERROR: Invalid POI Values\n";
										}
									}
									previousState = currentState;
									currentState = WAITING_FOR_DB_OBJECT_END;
								}
								else
								{
									previousState = currentState;
									currentState = WAITING_FOR_VALUE_SEPARATOR;
								}
							}
							else
							{
								error = CJson::JSON_ERR_EXPECT_ATTR_VALUE;
							}
							break;

						case WAITING_FOR_VALUE_SEPARATOR:
							if (event == CJsonToken::VALUE_SEPARATOR)
							{
								if (previousState == WAITING_FOR_DB_OBJECT_END)
								{
									previousState = currentState;
									currentState = WAITING_FOR_DB_OBJECT_BEGIN;
								}
								else if (previousState == WAITING_FOR_VALUE)
								{
									previousState = currentState;
									currentState = WAITING_FOR_ATTRIBUTE_NAME;
								}
								else if (previousEvent == CJsonToken::END_ARRAY)
								{
									previousState = currentState;
									currentState = WAITING_FOR_DB_NAME;
								}
								else
								{
									error = CJson::JSON_ERR_EXPECT_VALUE_SEPARATOR;
								}
							}
							else if (event == CJsonToken::END_ARRAY)
							{
								// This is end of the array object, prepare to read the new object
								previousState = currentState;
								currentState = WAITING_FOR_VALUE_SEPARATOR;
							}
							else if (event == CJsonToken::END_OBJECT)
							{
								// no more objects in the database
								previousState = currentState;
								currentState = WAITING_FOR_COMPLETION;
							}
							else
							{
								error = CJson::JSON_ERR_EXPECT_VALUE_SEPARATOR;
							}
							break;

						case WAITING_FOR_COMPLETION:
							// the file exhausted
							break;
					}

					// throw exception
					if (error > CJson::JSON_ERR_NO)
					{
						throw error;
					}

					previousEvent = event;
				}
			} while (this->m_pToken);
		}
		catch (jsonReadExceptions &ex)
		{
			this->exceptionHandler(ex, scanner.scannedLine());
		}
	}
	else
	{
		fileStream.clear();
		cout << "WARNING: Error opening the file to read - " << fileName << endl;
		ret = false;
	}

	fileStream.close();
	return ret;
}


void CJson::resetCurrentReadObjects()
{
	for (unsigned int Index = 0; Index < sizeof(this->m_currentObjectRead)/sizeof(this->m_currentObjectRead[0]); ++Index)
	{
		this->m_currentObjectRead[Index] = false;
	}
}


bool CJson::currentReadObject(string name)
{
	bool ret = true;

	if (!name.compare("waypoints"))
	{
		this->m_currentObjectRead[CJson::WAYPOINTS] 		= true;

	}
	else if (!name.compare("pois"))
	{
		this->m_currentObjectRead[CJson::POINT_OF_INTEREST] = true;
	}
	else
	{
		ret = false;
	}

	return ret;
}


bool CJson::expectedAttributeValue(string attributeName)
{
	bool ret = false;

	struct JsonAttribute
	{
		string 					name;
		CJsonToken::TokenType	tokenType;
		CPOI::AttributesType	attrType;
	};

	const JsonAttribute arributeLUT[] =
	{
			{"name",		CJsonToken::STRING, CPOI::NAME},
			{"latitude",	CJsonToken::NUMBER, CPOI::LATITUDE},
			{"longitude", 	CJsonToken::NUMBER, CPOI::LONGITUDE},
			{"type",		CJsonToken::STRING, CPOI::POI_TYPE},
			{"description",	CJsonToken::STRING, CPOI::DESCRIPTION},
	};

	this->m_exceptedTokenType 		= CJsonToken::JSON_NULL;
	this->m_exceptedAttributeType	= CPOI::INVALID_TYPE;

	for (unsigned int Index = 0; Index < sizeof(arributeLUT)/sizeof(JsonAttribute); ++Index)
	{
		if (!attributeName.compare(arributeLUT[Index].name))
		{
			this->m_exceptedTokenType 		= arributeLUT[Index].tokenType;
			this->m_exceptedAttributeType	= arributeLUT[Index].attrType;
			ret = true;
			break;
		}
	}

	return ret;
}


bool CJson::allAttributesRead()
{
	bool ret = false;

	if (this->m_currentObjectRead[WAYPOINTS])
	{
		ret  = 	this->m_isAttrAlreadyRead[CPOI::NAME] &&
				this->m_isAttrAlreadyRead[CPOI::LATITUDE] &&
				this->m_isAttrAlreadyRead[CPOI::LONGITUDE];
	}
	else if (this->m_currentObjectRead[POINT_OF_INTEREST])
	{
		ret  = 	this->m_isAttrAlreadyRead[CPOI::NAME] &&
				this->m_isAttrAlreadyRead[CPOI::LATITUDE] &&
				this->m_isAttrAlreadyRead[CPOI::LONGITUDE] &&
				this->m_isAttrAlreadyRead[CPOI::POI_TYPE] &&
				this->m_isAttrAlreadyRead[CPOI::DESCRIPTION];

	}
	else
	{
		ret = false;
	}

	return ret;
}


void CJson::resetAllAttributesRead()
{
	for (unsigned int Index = CPOI::INVALID_TYPE; Index < CPOI::MAX_TYPES; ++Index)
	{
		this->m_isAttrAlreadyRead[Index] = false;
	}
}


bool CJson::extractValue(string &name, double &latitude, double &longitude, CPOI::t_poi &type, string &description)
{
	CJsonStringToken *pTokenString;
	CJsonNumberToken *pTokenNumber;

	switch(this->m_exceptedAttributeType)
	{
		case CPOI::NAME:
			pTokenString = dynamic_cast<CJsonStringToken *>(this->m_pToken);
			if (pTokenString && !this->m_isAttrAlreadyRead[CPOI::NAME])
			{
				name = pTokenString->getValue();
				this->m_isAttrAlreadyRead[CPOI::NAME] = true;
			}
			else
			{
				this->m_isAttrAlreadyRead[CPOI::INVALID_TYPE] = true;
			}
			break;

		case CPOI::POI_TYPE:
			pTokenString = dynamic_cast<CJsonStringToken *>(this->m_pToken);
			if (pTokenString && !this->m_isAttrAlreadyRead[CPOI::POI_TYPE])
			{
				type = CPOI::getPoiType(pTokenString->getValue());
				this->m_isAttrAlreadyRead[CPOI::POI_TYPE] = true;
			}
			else
			{
				this->m_isAttrAlreadyRead[CPOI::INVALID_TYPE] = true;
			}
			break;

		case CPOI::DESCRIPTION:
			pTokenString = dynamic_cast<CJsonStringToken *>(this->m_pToken);
			if (pTokenString && !this->m_isAttrAlreadyRead[CPOI::DESCRIPTION])
			{
				description = pTokenString->getValue();
				this->m_isAttrAlreadyRead[CPOI::DESCRIPTION] = true;
			}
			else
			{
				this->m_isAttrAlreadyRead[CPOI::INVALID_TYPE] = true;
			}
			break;

		case CPOI::LATITUDE:
			pTokenNumber = dynamic_cast<CJsonNumberToken *>(this->m_pToken);
			if (pTokenNumber && !this->m_isAttrAlreadyRead[CPOI::LATITUDE])
			{
				latitude = pTokenNumber->getValue();
				this->m_isAttrAlreadyRead[CPOI::LATITUDE] = true;
			}
			else
			{
				this->m_isAttrAlreadyRead[CPOI::INVALID_TYPE] = true;
			}
			break;

		case CPOI::LONGITUDE:
			pTokenNumber = dynamic_cast<CJsonNumberToken *>(this->m_pToken);
			if (pTokenNumber && !this->m_isAttrAlreadyRead[CPOI::LONGITUDE])
			{
				longitude = pTokenNumber->getValue();
				this->m_isAttrAlreadyRead[CPOI::LONGITUDE] = true;
			}
			else
			{
				this->m_isAttrAlreadyRead[CPOI::INVALID_TYPE] = true;
			}
			break;

		case CPOI::INVALID_TYPE:
		default:
			this->m_isAttrAlreadyRead[CPOI::INVALID_TYPE] = true;
			break;
	}

	return (!this->m_isAttrAlreadyRead[CPOI::INVALID_TYPE]);
}


void CJson::exceptionHandler(jsonReadExceptions& ex, int lineNumber)
{
	string errorMsg;

	switch (ex)
	{
		case JSON_ERR_EXPECT_BEGIN_OBJECT:
			errorMsg = "ERROR: Expecting begining of an object";
		break;

		case JSON_ERR_EXPECT_DB_NAME_STRING:
			errorMsg = "ERROR: Expecting a Database name";
		break;

		case JSON_ERR_EXPECT_NAME_SEPARATOR:
			errorMsg = "ERROR: Expecting a name separator";
		break;

		case JSON_ERR_EXPECT_DB_ARRAY_BEGIN:
			errorMsg = "ERROR: Expecting a Database element array";
		break;

		case JSON_ERR_EXPECT_DB_OBJECT_BEGIN:
			errorMsg = "ERROR: Expecting a Database object begin";
		break;

		case JSON_ERR_EXPECT_DB_OBJECT_END:
			errorMsg = "ERROR: Expecting a Database object end";
		break;

		case JSON_ERR_EXPECT_ATTR_NAME:
			errorMsg = "ERROR: Expecting an attribute name";
		break;

		case JSON_ERR_EXPECT_ATTR_VALUE:
			errorMsg = "ERROR: Expecting an attribute value";
		break;

		case JSON_ERR_EXPECT_VALUE_SEPARATOR:
			errorMsg = "ERROR: Expecting a value separator";
		break;

		case JSON_ERR_ILLEGAL_CHARACTER:
			errorMsg = "ERROR: Illegal character";
		break;

		case JSON_ERR_NO:
		default:
			errorMsg = "ERROR: Unknown";
		break;
	}

	cout << errorMsg << " at line : " << lineNumber << endl;
}

