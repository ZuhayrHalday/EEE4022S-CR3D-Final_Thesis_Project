cmake_minimum_required(VERSION 3.20)
project(MuonBar LANGUAGES CXX)

# Path to your Geant4 *install*'s CMake package:
set(Geant4_DIR "C:/Users/Zuhayr/Desktop/Geant4/geant4-v11.3.2/program_files/lib/cmake/Geant4")

# Ask for UI and visualization components (portable across versions)
find_package(Geant4 CONFIG REQUIRED COMPONENTS ui_all vis_all)

# Your sources/headers
file(GLOB sources ${PROJECT_SOURCE_DIR}/src/*.cc)
file(GLOB headers ${PROJECT_SOURCE_DIR}/include/*.hh)

add_executable(MuonBar ${sources} ${headers})
target_compile_features(MuonBar PRIVATE cxx_std_17)

# Link with the aggregate libs Geant4 provides
target_link_libraries(MuonBar PRIVATE ${Geant4_LIBRARIES})

# Include dirs (modern Geant4 exports this variable)
target_include_directories(MuonBar PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${Geant4_INCLUDE_DIRS}
)

