# script to automatically download and install dependencies for SrModLdr
# Tested with PowerShell 2.0 on Windows 7 with Visual Studio 2017 on a 32-bit system
# Should work down to Visual Studio 2010 and Windows XP. Not sure if 64-bit works.

if([IntPtr]::size -eq 4){
    #$url_7zip = "http://www.7-zip.org/a/7z1604.msi" 
    #$url_cmake = "https://cmake.org/files/v3.7/cmake-3.7.2-win32-x86.msi"
    $url_cmake = "https://cmake.org/files/v3.7/cmake-3.7.2-win32-x86.zip"
} else {
    #$url_7zip = "http://www.7-zip.org/a/7z1604-x64.msi"
    #$url_cmake = "https://cmake.org/files/v3.7/cmake-3.7.2-win64-x64.msi"
    $url_cmake = "https://cmake.org/files/v3.7/cmake-3.7.2-win64-x64.zip"
}
$url_7zip = "http://www.7-zip.org/a/7za920.zip" #7-Zip 9.20 Portable
$url_vswhere = "https://github.com/Microsoft/vswhere/releases/download/1.0.55/vswhere.exe"
$url_sqlite3_src = "https://sqlite.org/2017/sqlite-amalgamation-3170000.zip"
$url_sqlite3_bin = "https://sqlite.org/2017/sqlite-dll-win32-x86-3170000.zip"
$url_jansson = "http://www.digip.org/jansson/releases/jansson-2.10.tar.gz"
$url_zlib = "http://zlib.net/zlib1211.zip"
$url_libarchive = "https://github.com/libarchive/libarchive/zipball/master"

$client = new-object System.Net.WebClient 
$shell = new-object -com shell.application

md -Force tmp
md -Force lib
md -Force include
$tmpdir = Join-Path $pwd "tmp"


# https://www.howtogeek.com/tips/how-to-extract-zip-files-using-powershell/
function Expand-ZIPFile($file, $destination) {
    mkdir -Force $destination
    $zip = $shell.NameSpace($file)
    foreach($item in $zip.items()) {
        $shell.Namespace($destination).copyhere($item)
    }
}

# http://stackoverflow.com/a/2128350
function Invoke-BatchFile
{
   param([string]$Path, [string]$Parameters)  

   $tempFile = [IO.Path]::GetTempFileName()  

   ## Store the output of cmd.exe.  We also ask cmd.exe to output   
   ## the environment table after the batch file completes  
   cmd.exe /c " `"$Path`" $Parameters && set > `"$tempFile`" " 

   ## Go through the environment variables in the temp file.  
   ## For each of them, set the variable in our local environment.  
   Get-Content $tempFile | Foreach-Object {   
       if ($_ -match "^(.*?)=(.*)$")  
       { 
           Set-Content "env:\$($matches[1])" $matches[2]  
       } 
   }  

   Remove-Item $tempFile
}

function Download-ZIPFile($url, $dirname, $basedir){
    $name = $url.split('/')[-1]
    $path = Join-Path $basedir (Split-Path -leaf $name) 

    # Check if already downloaded
    if(!(Test-Path $path)){
        Write-Host ("Downloading " + $dirname + "...")
        $client.DownloadFile($url, $path)
    }
    if(!(Test-Path (Join-Path $basedir $dirname) )){
        Expand-ZIPFile -File $path -Destination (Join-Path $basedir $dirname)
    }
}



# Find 7-Zip install path in registry
$ipath_7zip = (Get-ItemProperty -Path HKLM:\SOFTWARE\7-Zip\ -name Path).Path

# Install 7-Zip portable from internet if not found
if (!$ipath_7zip){
    Download-ZIPFile -url $url_7zip -basedir $tmpdir -dirname "7-Zip"
    $ipath_7zip = Join-Path (Join-Path $tmpdir "7-Zip") "7za.exe"

} else {
    # Get 7-Zip executable path
    $ipath_7zip = Join-Path $ipath_7zip "7z.exe"
}



# Check if CMake exists
$ipath_cmake = (Get-ItemProperty -Path HKLM:\SOFTWARE\Kitware\CMake -name InstallDir).InstallDir

# Install CMake ZIP from internet if not found
if (!$ipath_cmake){
    Download-ZIPFile -url $url_cmake -basedir $tmpdir -dirname "CMake"
    $ipath_cmake = Join-Path (Join-Path $tmpdir "CMake") ([io.path]::GetFileNameWithoutExtension($name)  + "")
}

# Get CMake executable path
$ipath_cmake = Join-Path (Join-Path $ipath_cmake "bin") "cmake.exe"

# Download vswhere.exe from GitHub
$ipath_vswhere = Join-Path $tmpdir "vswhere.exe"
if(!(Test-Path $ipath_vswhere)){
    Write-Host "Downloading vswhere.exe..."
    $client.DownloadFile($url_vswhere, $ipath_vswhere)
}

# Get Visual Studio install directory
$vsPath = & $ipath_vswhere ("-legacy", "-latest", "-format", "value", "-property", "installationPath")
$vsVer =  (& $ipath_vswhere ("-legacy", "-latest", "-format", "value", "-property", "installationVersion")).split(".")[0]
if(!$vsPath){
    exit -1
}

# Init VS command line tools (only tested on VS2017!)
if ($vsVer -eq "15"){ # VS 2017
    $ToolPath = (Join-Path (Join-Path $vsPath "Common7") "Tools")
    $vsBat = "VsDevCmd.bat"
} elseif ($vsVer -eq "14"){ # VS 2015
    $ToolPath = (Join-Path (Join-Path $vsPath "Common7") "Tools")
    $vsBat = "vsvars32.bat"
} else {
    $ToolPath = (Join-Path $vsPath "VC")
    $vsBat = "vcvarsall.bat" # VS 2010/2013
}
Invoke-BatchFile (Join-Path $ToolPath $vsBat)

## JANSSON 
# Download jansson source
$name = $url_jansson.split('/')[-1]
$path = Join-Path $tmpdir (Split-Path -leaf $name) 
$janssonver = ($name.split('.')[0] + "." + $name.split('.')[1])
if(!(Test-Path $path)){
    Write-Host "Downloading Jansson..."
    $client.DownloadFile($url_jansson, $path)
}

# Extract jansson source
if(!(Test-Path (Join-Path $tmpdir 'jansson'))){
    Write-Host "Extracting Jansson..."
    $prm1 = 'x', $path, ('-o' + $tmpdir), '-y'
    $prm2 = 'x', ((Join-Path $tmpdir $janssonver) + ".tar"), '-aoa', '-ttar', ('-o' + (Join-Path $tmpdir 'jansson')), '-y'
    & $ipath_7zip $prm1 
    & $ipath_7zip $prm2
    rm ((Join-Path $tmpdir $janssonver) + ".tar")
}

# Generate jansson solution
mkdir (Join-Path (Join-Path (Join-Path $tmpdir "jansson") $janssonver) "bin") -Force
cd (Join-Path (Join-Path (Join-Path $tmpdir "jansson") $janssonver) "bin")
& $ipath_cmake ..

# Build jansson solution
devenv /Build "Release|Win32" ALL_BUILD.vcxproj

# Copy output to lib/ and include/
cp lib/Release/jansson.lib ../../../../lib/jansson.lib
cp include/jansson.h ../../../../include/jansson.h
cp include/jansson_config.h ../../../../include/jansson_config.h
# Copy dll to src/include
# (Where is the dll?)

cd ../../../../ # Exit bin directory

## SQLITE 3
# Download source and extract
Download-ZIPFile -url $url_sqlite3_src -basedir $tmpdir -dirname "SQLite3"

# Copy sqlite.h to include dir
cd (Join-Path (Join-Path $tmpdir SQLite3) ([io.path]::GetFileNameWithoutExtension($url_sqlite3_src.split('/')[-1])  + ""))
cp sqlite3.h ../../../include/sqlite3.h
cd ../../../

# Download binary and extract
Download-ZIPFile -url $url_sqlite3_bin -basedir $tmpdir -dirname "SQLite3-Bin"

# Get library from DLL
cd (Join-Path $tmpdir "SQLite3-Bin")
LIB /DEF:sqlite3.def
cp sqlite3.dll ../../../include/sqlite3.dll # Copy DLL to /src/include
cp sqlite3.lib ../../lib/sqlite3.lib
cd ../../


## ZLIB
Download-ZIPFile -url $url_zlib -basedir $tmpdir -dirname "zlib"

# Compile with CMake
cd (Join-Path $tmpdir "zlib")
cd ((dir | Select-Object name -first 1).name) # Enter directory with weird name
md bin -Force
cd bin

& $ipath_cmake ..
devenv /Build "Release|Win32" ALL_BUILD.vcxproj

# Copy results to lib/ and include/
cp Release/zlib.lib ../../../../lib/zlib.lib
cp Release/zlib.dll ../../../../lib/zlib.dll # for libarchive cmake
cp Release/zlib.dll ../../../../../include/zlib.dll # Copy DLL to src/include
cp zconf.h ../../../../include/zconf.h
cd ..
cp zlib.h ../../../include/zlib.h


cd ../../../

## libarchive

# Filename on server breaks function
$name = "libarchive.zip"
$path = Join-Path $tmpdir $name
if(!(Test-Path $path)){
    Write-Host ("Downloading libarchive...")
    $client.DownloadFile($url_libarchive, $path)
}
if(!(Test-Path (Join-Path $tmpdir "libarchive") )){
    Expand-ZIPFile -File $path -Destination (Join-Path $tmpdir "libarchive")
}

# Compile with CMake
cd (Join-Path $tmpdir "libarchive")
cd ((dir | Select-Object name -first 1).name) # Enter directory with really weird name

md bin -Force
cd bin

& $ipath_cmake ("-D", "CMAKE_PREFIX_PATH:FILEPATH=../../../../", "../")
devenv /Build "Release|Win32" ALL_BUILD.vcxproj

# Copy output to root build dir
cp libarchive/Release/archive.lib ../../../../lib/archive.lib
cd ..
cp libarchive/archive.h ../../../include/archive.h
# (TODO: Figure out how to build DLL)

cd ../../../

Write-Host "Done! Ready to build SrModLdr!"