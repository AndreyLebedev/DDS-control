// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// ODC
#include "DDSSubmit.h"
#include "BuildConstants.h"
#include "Logger.h"
// BOOST
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
// STD
#include <set>

using namespace odc;
using namespace odc::core;
using namespace std;
namespace fs = boost::filesystem;
namespace pt = boost::property_tree;

//
// CDDSSubmit::SParams
//

CDDSSubmit::SParams::SParams()
{
}

CDDSSubmit::SParams::SParams(
    const string& _rmsPlugin, const string& _configFile, size_t _numAgents, size_t _numSlots, size_t _requiredNumSlots)
    : m_rmsPlugin(_rmsPlugin)
    , m_configFile(_configFile)
    , m_numAgents(_numAgents)
    , m_numSlots(_numSlots)
    , m_requiredNumSlots(_requiredNumSlots)
{
}

void CDDSSubmit::SParams::initFromXML(istream& _stream)
{
    pt::ptree pt;
    pt::read_xml(_stream, pt, pt::xml_parser::no_comments);
    initFromPT(pt);
}

void CDDSSubmit::SParams::initFromPT(const pt::ptree& _pt)
{
    // TODO: FIXME: <configContent> is not yet defined
    // To support it we need to create a temporary file with configuration content and use it as config file.

    // Do some basic validation of the input tree.
    // Only valid tags are allowed.
    set<string> validTags{ "rms", "configFile", "agents", "slots", "requiredSlots" };
    for (const auto& v : _pt)
    {
        if (validTags.count(v.first.data()) == 0)
        {
            stringstream ss;
            ss << "Failed to init from property tree. Unknown key " << quoted(v.first.data());
            throw runtime_error(ss.str());
        }
    }
    m_rmsPlugin = _pt.get<string>("rms", "");
    m_configFile = _pt.get<string>("configFile", "");
    m_numAgents = _pt.get<size_t>("agents", 0);
    m_numSlots = _pt.get<size_t>("slots", 0);
    m_requiredNumSlots = _pt.get<size_t>("requiredSlots", 0);
}

//
// CDDSSubmit
//

CDDSSubmit::CDDSSubmit()
{
    // Register default plugins
    registerDefaultPlugin("odc-rp-same");
}

void CDDSSubmit::registerDefaultPlugin(const std::string& _name)
{
    try
    {
        fs::path pluginPath{ kODCBinDir };
        pluginPath /= _name;
        registerPlugin(_name, pluginPath.string());
    }
    catch (const exception& _e)
    {
        OLOG(ESeverity::error) << "Unable to register default resource plugin " << quoted(_name) << ": " << _e.what();
    }
}

CDDSSubmit::SParams CDDSSubmit::makeParams(const string& _plugin,
                                           const string& _resources,
                                           const partitionID_t& _partitionID,
                                           runNr_t _runNr)
{
    CDDSSubmit::SParams params;
    stringstream ss{ execPlugin(_plugin, _resources, _partitionID, _runNr) };
    params.initFromXML(ss);
    return params;
}

//
// Misc
//

namespace odc::core
{
    ostream& operator<<(ostream& _os, const CDDSSubmit::SParams& _params)
    {
        return _os << "CDDSSubmit::SParams: rmsPlugin=" << quoted(_params.m_rmsPlugin)
                   << "; numAgents=" << _params.m_numAgents << "; numSlots=" << _params.m_numSlots
                   << "; configFile=" << quoted(_params.m_configFile)
                   << "; requiredNumSlots=" << _params.m_requiredNumSlots;
    }
} // namespace odc::core
