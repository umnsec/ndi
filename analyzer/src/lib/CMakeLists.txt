set (AnalyzerSourceCodes
	Config.hh
	Common.hh
	Common.cc
	Tools.hh
	Tools.cc
	Analyzer.hh
	Analyzer.cc
	RIDCommon.hh
	RIDCommon.cc
	FunctionPathsPass.hh
	FunctionPathsPass.cc
	CallGraph.hh
	CallGraph.cc
	ExpressionGraph.hh
	ExpressionGraph.cc
	PEGNode.hh
	PEGNode.cc
	PEGZ3.hh
	PEGZ3.cc
	ValueFlowPass.hh
	ValueFlowPass.cc
	CriticalVariable.hh
	CriticalVariable.cc
	Distinguisher.hh
	Distinguisher.cc
	PEGFunctionBuilder.hh
	PEGFunctionBuilder.cc
	ReleaseIdentification.hh
	ReleaseIdentification.cc
	)

file(COPY configs/ DESTINATION configs)

set(CMAKE_MACOSX_RPATH 0)

# Build libraries.
add_library (AnalyzerObj OBJECT ${AnalyzerSourceCodes})
add_library (Analyzer SHARED $<TARGET_OBJECTS:AnalyzerObj>)
add_library (AnalyzerStatic STATIC $<TARGET_OBJECTS:AnalyzerObj>)

# Build executable.
set (EXECUTABLE_OUTPUT_PATH ${ANALYZER_BINARY_DIR})
link_directories (${ANALYZER_BINARY_DIR}/lib)
add_executable(kanalyzer ${AnalyzerSourceCodes})
target_link_libraries(kanalyzer 
  LLVMAsmParser 
  LLVMSupport 
  LLVMCore 
  LLVMAnalysis
  LLVMIRReader
  AnalyzerStatic
  ${Z3_LIBRARIES})

