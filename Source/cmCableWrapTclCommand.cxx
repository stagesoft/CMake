/*=========================================================================

  Program:   Insight Segmentation & Registration Toolkit
  Module:    $RCSfile$
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

Copyright (c) 2001 Insight Consortium
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

 * The name of the Insight Consortium, nor the names of any consortium members,
   nor of any contributors, may be used to endorse or promote products derived
   from this software without specific prior written permission.

  * Modified source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS ``AS IS''
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/
#include "cmCableWrapTclCommand.h"
#include "cmCacheManager.h"
#include "cmTarget.h"
#include "cmGeneratedFileStream.h"
#include <strstream>

cmCableWrapTclCommand::cmCableWrapTclCommand():
  m_CableClassSet(NULL)
{
}

cmCableWrapTclCommand::~cmCableWrapTclCommand()
{
  if(m_CableClassSet)
    {
    delete m_CableClassSet;
    }
}



// cmCableWrapTclCommand
bool cmCableWrapTclCommand::Invoke(std::vector<std::string>& args)
{
  if(args.size() < 2)
    {
    this->SetError("called with incorrect number of arguments");
    return false;
    }
  
  // Prepare to iterate through the arguments.
  std::vector<std::string>::const_iterator arg = args.begin();
  
  // The first argument is the name of the target.
  m_TargetName = *arg++;
  
  // Create the new class set.
  m_CableClassSet = new cmCableClassSet(m_TargetName.c_str());
  
  // Add all the regular entries.
  for(; (arg != args.end()) && (*arg != "SOURCES_BEGIN"); ++arg)
    {
    m_CableClassSet->ParseAndAddElement(arg->c_str(), m_Makefile);
    }
  
  // Add any sources that are associated with all the members.
  if(arg != args.end())
    {
    for(++arg; arg != args.end(); ++arg)
      {
      m_CableClassSet->AddSource(arg->c_str());
      }
    }  

  this->GenerateCableFiles();
  
  // Add the source list to the target.
  m_Makefile->GetTargets()[m_TargetName.c_str()].GetSourceLists().push_back(m_TargetName);
  
  return true;
}


/**
 * Generate the files that CABLE will use to generate the wrappers.
 */
void cmCableWrapTclCommand::GenerateCableFiles() const
{
  // Setup the output directory.
  std::string outDir = m_Makefile->GetCurrentOutputDirectory();
  cmSystemTools::MakeDirectory((outDir+"/Tcl").c_str());

  std::string packageConfigName = outDir+"/Tcl/"+m_TargetName+"_config.xml";
  std::string packageTclName = outDir+"/Tcl/"+m_TargetName+"_tcl";
  
  // Generate the main package configuration file for CABLE.
  cmGeneratedFileStream packageConfig(packageConfigName.c_str());
  if(packageConfig)
    {
    packageConfig <<
      "<CableConfiguration package=\"" << m_TargetName.c_str() << "\">\n"
      "  <Groups>\n";
    for(unsigned int i=0; i < m_CableClassSet->Size(); ++i)
      {
      packageConfig <<
        "  " << m_TargetName.c_str() << "_" << i << "\n";
      }
    packageConfig <<
      "  </Groups>\n"
      "</CableConfiguration>\n";
  
    packageConfig.close();
    }
  else
    {
    cmSystemTools::Error("Error opening CABLE configuration file for writing: ",
                         packageConfigName.c_str());
    }

  {
  std::string command = "${CABLE}";
  m_Makefile->ExpandVariablesInString(command);
  std::vector<std::string> depends;
  depends.push_back(command);
  command = cmSystemTools::EscapeSpaces(command.c_str());
  command += " "+packageConfigName+" -tcl "+packageTclName+".cxx";
  
  depends.push_back(packageConfigName);
  
  std::vector<std::string> outputs;
  outputs.push_back(packageTclName+".cxx");
  
  m_Makefile->AddCustomCommand(packageConfigName.c_str(),
                               command.c_str(),
                               depends,
                               outputs, m_TargetName.c_str());
  }
  
  // Add the generated source to the package's source list.
  cmSourceFile file;
  file.SetName(packageTclName.c_str(), outDir.c_str(), "cxx", false);
  // Set dependency hints.
  file.GetDepends().push_back("wrapCalls.h");
  m_Makefile->AddSource(file, m_TargetName.c_str());
  
  unsigned int index = 0;
  for(cmCableClassSet::CableClassMap::const_iterator
        c = m_CableClassSet->Begin(); c != m_CableClassSet->End(); ++c, ++index)
    {
    this->GenerateCableClassFiles(c->first.c_str(), c->second, index);
    }
  
}


void cmCableWrapTclCommand::GenerateCableClassFiles(const char* name,
                                                    const cmCableClass& c,
                                                    unsigned int index) const
{
  std::strstream indexStrStream;
  indexStrStream << index << std::ends;
  std::string indexStr = indexStrStream.str();
  
  std::string outDir = m_Makefile->GetCurrentOutputDirectory();  
  
  std::string className = name;
  std::string groupName = m_TargetName+"_"+indexStr;
  std::string classConfigName = outDir+"/Tcl/"+groupName+"_config_tcl.xml";
  std::string classCxxName = outDir+"/Tcl/"+groupName+"_cxx.cxx";
  std::string classXmlName = outDir+"/Tcl/"+groupName+"_cxx.xml";
  std::string classTclName = outDir+"/Tcl/"+groupName+"_tcl";
  
  cmGeneratedFileStream classConfig(classConfigName.c_str());
  if(classConfig)
    {
    classConfig <<
      "<CableConfiguration source=\"" << classXmlName.c_str() << "\" "
      "group=\"" << groupName.c_str() << "\">\n";
    for(cmCableClass::Sources::const_iterator source = c.SourcesBegin();
        source != c.SourcesEnd(); ++source)
      {
      classConfig <<
        "  <Header name=\"" << source->c_str() << "\"/>\n";
      }
    classConfig <<
      "  <WrapperSet>\n"
      "  _wrap_::wrapper::Wrapper\n"
      "  </WrapperSet>\n"
      "</CableConfiguration>\n";
  
    classConfig.close();
    }
  else
    {
    cmSystemTools::Error("Error opening CABLE configuration file for writing: ",
                         classConfigName.c_str());
    }

  cmGeneratedFileStream classCxx(classCxxName.c_str());
  if(classCxx)
    {
    for(cmCableClass::Sources::const_iterator source = c.SourcesBegin();
        source != c.SourcesEnd(); ++source)
      {
      classCxx <<
        "#include \"" << source->c_str() << "\"\n";
      }
    classCxx <<
      "\n"
      "namespace _wrap_\n"
      "{\n"
      "\n"
      "struct wrapper\n"
      "{\n"
      "  typedef ::" << className.c_str() << " Wrapper;\n"
      "};\n"
      "\n"
      "template <typename T> void Eat(T) {}\n"
      "\n"
      "void InstantiateMemberDeclarations()\n"
      "{\n"
      "  Eat(sizeof(wrapper::Wrapper));\n"
      "}\n"
      "\n"
      "}\n";
    
    classCxx.close();
    }
  else
    {
    cmSystemTools::Error("Error opening file for writing: ",
                         classCxxName.c_str());
    }
  
  {
  std::string command = "${GCCXML}";
  m_Makefile->ExpandVariablesInString(command);
  // Only add the rule if GCC-XML is available.
  if((command != "") && (command != "${GCCXML}"))
    {
    std::vector<std::string> depends;
    depends.push_back(command);
    command = cmSystemTools::EscapeSpaces(command.c_str());
    command += " ${CMAKE_CXXFLAGS} ${INCLUDE_FLAGS} -fsyntax-only -fxml=" + classXmlName + " " + classCxxName;
    
    std::vector<std::string> outputs;
    outputs.push_back(classXmlName);
    
    m_Makefile->AddCustomCommand(classCxxName.c_str(),
                                 command.c_str(),
                                 depends,
                                 outputs, m_TargetName.c_str());
    }
  }

  {
  std::string command = "${CABLE}";
  m_Makefile->ExpandVariablesInString(command);
  std::vector<std::string> depends;
  depends.push_back(command);
  command = cmSystemTools::EscapeSpaces(command.c_str());
  command += " "+classConfigName+" -tcl "+classTclName+".cxx";
  
  depends.push_back(classConfigName);
  depends.push_back(classXmlName);
  
  std::vector<std::string> outputs;
  outputs.push_back(classTclName+".cxx");
  
  m_Makefile->AddCustomCommand(classConfigName.c_str(),
                               command.c_str(),
                               depends,
                               outputs, m_TargetName.c_str());
  }

  // Add the generated source to the package's source list.
  cmSourceFile file;
  file.SetName(classTclName.c_str(), outDir.c_str(), "cxx", false);
  // Set dependency hints.
  for(cmCableClass::Sources::const_iterator source = c.SourcesBegin();
      source != c.SourcesEnd(); ++source)
    {
    file.GetDepends().push_back(*source);
    }
  file.GetDepends().push_back("wrapCalls.h");
  m_Makefile->AddSource(file, m_TargetName.c_str());
}
