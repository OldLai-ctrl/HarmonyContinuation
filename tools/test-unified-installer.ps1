param(
 [string]$PackageDirectory='build-installer/rc-0.9.0-rc.3-unified',
 [string]$TestRoot='build-installer/unified-smoke',
 [string]$BuildDirectory='build-v3-plugin',
 [string]$PreviousPackageDirectory='',
 [string]$PreviousVersion='',
 [string]$Iscc='build-installer/tools/Inno/ISCC.exe')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
function ResolveRepoPath([string]$path) {
 if ([IO.Path]::IsPathRooted($path)) { return [IO.Path]::GetFullPath($path) }
 return [IO.Path]::GetFullPath((Join-Path $repo $path))
}
function IsWorkspaceChild([string]$path) {
 $workspace=[IO.Path]::GetFullPath((Split-Path -Parent $repo)).TrimEnd([IO.Path]::DirectorySeparatorChar)
 return $path.StartsWith($workspace+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)
}
$root=ResolveRepoPath $TestRoot
if (!(IsWorkspaceChild $root) -or (Test-Path -LiteralPath $root)) { throw 'A fresh workspace-owned isolated root is required.' }
$reg='HKCU:/Software/Microsoft/Windows/CurrentVersion/Uninstall/HarmonyContinuation-Application-IsolatedTest_is1'
$oldReg='HKCU:/Software/Microsoft/Windows/CurrentVersion/Uninstall/HarmonyContinuation-FactoryLibrary-IsolatedTest_is1'
if ((Test-Path $reg) -or (Test-Path $oldReg)) { throw 'An isolated registration already exists; do not overwrite it.' }
$buildPath=ResolveRepoPath $BuildDirectory
$packagePath=ResolveRepoPath $PackageDirectory
$moduleInfoTool=Join-Path $buildPath 'bin/moduleinfotool.exe'
$moduleInfoAvailable=Test-Path -LiteralPath $moduleInfoTool -PathType Leaf
$headerPath=Join-Path $buildPath 'generated/ProductVersionGenerated.h'
if (!(Test-Path -LiteralPath $headerPath)) { throw "Missing build identity header: $headerPath" }
$header=Get-Content -LiteralPath $headerPath -Raw
if ($header -notmatch 'HC_PRODUCT_FACTORY_LIBRARY_VERSION\s+(\d+)') { throw 'Missing Factory version.' }
$libraryVersion=$Matches[1]
$stage=Join-Path $packagePath "stage-$libraryVersion"
$manager=Join-Path $buildPath 'Release/library_manager.exe'
if (!(Test-Path -LiteralPath $manager)) { $manager=Join-Path $buildPath 'library_manager.exe' }
if (!(Test-Path -LiteralPath $manager)) { throw "Missing library_manager.exe in build: $buildPath" }
if (!(Test-Path -LiteralPath $stage)) { throw "Missing current package stage: $stage" }
$info=& $manager inspect (Join-Path $stage 'factory.db')
if ($LASTEXITCODE -ne 0 -or $info -notmatch 'library_version=(\d+)') { throw 'Missing Factory version from package stage.' }
if ($Matches[1] -ne $libraryVersion) { throw 'Build and package Factory versions differ.' }
$currentLibraryVersion=$Matches[1]
if ($info -notmatch 'progressions=(\d+)') { throw 'Missing Factory entry count.' }
$entryCount=$Matches[1]
$versionMatch=[regex]::Match($header,'HC_PRODUCT_VERSION\s+"([^"]+)"')
if (!$versionMatch.Success) { throw 'Missing product version from build identity header.' }
$version=$versionMatch.Groups[1].Value
$isccPath=ResolveRepoPath $Iscc
if (!(Test-Path -LiteralPath $isccPath -PathType Leaf)) { throw "Missing Inno Setup compiler: $isccPath" }
$factoryHash=(Get-FileHash -LiteralPath (Join-Path $stage 'factory.db')).Hash
$legacyDb=Join-Path $buildPath 'factory-v2.db'
$legacyHash=if (Test-Path -LiteralPath $legacyDb) { (Get-FileHash -LiteralPath $legacyDb).Hash } else { '' }
$previousStage=''
$previousLibraryVersion=''
$previousEntryCount=''
$previousFactoryHash=''
$previousLegacyHash=''
if ($PreviousPackageDirectory) {
 if (!$PreviousVersion) { throw 'PreviousVersion is required with PreviousPackageDirectory.' }
 $previousPackagePath=ResolveRepoPath $PreviousPackageDirectory
 if (!(Test-Path -LiteralPath $previousPackagePath -PathType Container)) { throw "Missing previous payload directory: $previousPackagePath" }
 if ((Split-Path -Leaf $previousPackagePath) -like 'stage-*') {
  $previousStage=$previousPackagePath
 } else {
  $previousStages=@(Get-ChildItem -LiteralPath $previousPackagePath -Directory -Filter 'stage-*')
  if ($previousStages.Count -ne 1) { throw 'Previous package must contain exactly one extracted stage-* directory.' }
  $previousStage=$previousStages[0].FullName
 }
 $previousModuleInfoPath=Join-Path $previousStage 'HarmonyContinuation.vst3/Contents/Resources/moduleinfo.json'
 if (!(Test-Path -LiteralPath $previousModuleInfoPath)) { throw 'Previous stage is missing moduleinfo.json.' }
 $previousInfo=Get-Content -LiteralPath $previousModuleInfoPath -Raw
 if ($previousInfo -notmatch '"Version":\s*"([^"]+)"' -or $Matches[1] -ne $PreviousVersion) { throw 'Previous payload version mismatch.' }
 $previousDatabase=Join-Path $previousStage 'factory.db'
 $previousInspection=& $manager inspect $previousDatabase
 if ($LASTEXITCODE -ne 0 -or $previousInspection -notmatch 'library_version=(\d+)') { throw 'Previous Factory inspection failed.' }
 $previousLibraryVersion=$Matches[1]
 if ($previousLibraryVersion -ne $currentLibraryVersion) { throw 'Previous and current payloads must use the same Factory content version.' }
 if ($previousInspection -notmatch 'progressions=(\d+)') { throw 'Previous Factory entry count is missing.' }
 $previousEntryCount=$Matches[1]
 $previousFactoryHash=(Get-FileHash -LiteralPath $previousDatabase).Hash
 if ($previousFactoryHash -ne $factoryHash) { throw 'Same-Factory upgrade fixture required.' }
 $previousLegacyDb=Join-Path $previousStage 'official-factory/factory-v2.db'
 if (Test-Path -LiteralPath $previousLegacyDb) { $previousLegacyHash=(Get-FileHash -LiteralPath $previousLegacyDb).Hash }
}
New-Item -ItemType Directory -Path $root | Out-Null
$script:checks=0
function Check([bool]$ok,[string]$message) { if (!$ok) { throw $message }; $script:checks++ }
function Compile([string]$where,[bool]$legacy=$false,[bool]$library=$false,[bool]$previous=$false) {
 $output=Join-Path $where $(if($previous){'packages-previous'}else{'packages'}); New-Item -ItemType Directory -Force -Path $output | Out-Null
 $source=Join-Path $repo 'packaging/UnifiedSetup.iss'; $payload=$stage;$compileVersion=$version
 $compileLibraryVersion=$libraryVersion;$compileEntryCount=$entryCount;$compileFactoryHash=$factoryHash;$compileLegacyHash=$legacyHash
 if ($previous) {
  $payload=$previousStage
  $compileVersion=$PreviousVersion
  $compileLibraryVersion=$previousLibraryVersion;$compileEntryCount=$previousEntryCount;$compileFactoryHash=$previousFactoryHash
  $compileLegacyHash=$previousLegacyHash
  $source=Join-Path $output 'UnifiedSetup.iss'
  $previousSource=& git -c ('safe.directory='+$repo.Replace('\','/')) -C $repo show 'v0.10.0:packaging/UnifiedSetup.iss'
  if ($LASTEXITCODE -ne 0 -or !$previousSource) { throw 'Cannot load the exact v0.10.0 installer source.' }
  $text=[string]::Join([Environment]::NewLine,@($previousSource))
  if ($text -notmatch 'InfoBeforeFile=installer-info\.txt' -or $text -notmatch 'MessagesFile: "ChineseSimplified\.isl"') { throw 'The v0.10.0 installer source resource paths changed unexpectedly.' }
  $text=$text.Replace('InfoBeforeFile=installer-info.txt',('InfoBeforeFile="'+$repo+'\packaging\installer-info.txt"'))
  $text=$text.Replace('MessagesFile: "ChineseSimplified.isl"',('MessagesFile: "'+$repo+'\packaging\ChineseSimplified.isl"'))
  [IO.File]::WriteAllText($source,$text,[Text.UTF8Encoding]::new($false))
 }
 if ($legacy) {
  $payload=ResolveRepoPath 'build-installer/rc-0.9.0-rc.2/stage-3'
  $source=Join-Path $output 'Legacy.iss'
  # Only the isolated copy gets per-user uninstall records. Git history stays intact.
  $text=(Get-Content (Join-Path $repo 'packaging/HarmonyContinuation.iss') -Raw).Replace('CreateUninstallRegKey=no','CreateUninstallRegKey=yes')
  $text=$text.Replace('InfoBeforeFile=installer-info.txt',('InfoBeforeFile='+$repo+'\packaging\installer-info.txt')).Replace('MessagesFile: "ChineseSimplified.isl"',('MessagesFile: "'+$repo+'\packaging\ChineseSimplified.isl"'))
  [IO.File]::WriteAllText($source,$text,[Text.UTF8Encoding]::new($false))
 }
 $hcArgs=@('/Qp',"/DStageDir=$payload","/DOutputPath=$output","/DProductVersion=$compileVersion","/DLibraryVersion=$(if($legacy){3}else{$compileLibraryVersion})","/DFactoryEntryCount=$compileEntryCount","/DFactoryHash=$compileFactoryHash","/DLegacyFactoryHash=$compileLegacyHash","/DTestRoot=$where")
 if ($library) { $hcArgs+='/DLibraryOnly' }
 & $isccPath @hcArgs $source *> (Join-Path $output ('compile-'+$legacy+'-'+$library+'-'+$previous+'.log'))
 if ($LASTEXITCODE -ne 0) { throw 'Isolated compiler failed; see compile log.' }
 if (!$legacy) { return Join-Path $output 'HarmonyContinuation-Setup-IsolatedTest.exe' }
 if ($library) { return Join-Path $output 'HarmonyContinuation-Library-3-Setup-IsolatedTest.exe' }
 return Join-Path $output "HarmonyContinuation-$version-Setup-IsolatedTest.exe"
}
function Run([string]$exe,[string]$log,[string[]]$extra=@(),[object]$success=$true) {
 $hcArgs=@('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/LANG=english',('/LOG="'+$log+'"'))+$extra
 $process=Start-Process -FilePath $exe -ArgumentList $hcArgs -WindowStyle Hidden -Wait -PassThru
 if ($null -ne $success) { Check (($process.ExitCode -eq 0) -eq [bool]$success) "Unexpected exit $($process.ExitCode): $log" }
 return $process.ExitCode
}
function ValidateInstalledModule([string]$label,[string]$modulePath,[string]$moduleVersion,[string]$appDirectory) {
	if (!$moduleInfoAvailable) { throw "Missing moduleinfotool: $moduleInfoTool" }
	$infoPath=Join-Path $root ("moduleinfo-$label.json")
	$createLog=Join-Path $root ("moduleinfo-$label-create.log")
	$validateLog=Join-Path $root ("moduleinfo-$label-validate.log")
	$originalPath=$env:PATH
	$pathEntries=@((Split-Path $moduleInfoTool),(Split-Path $modulePath),$appDirectory)
	try {
		$env:PATH=([string]::Join(';',@($pathEntries | Where-Object { $_ })+$originalPath))
		$createOutput=& $moduleInfoTool -create -version $moduleVersion -path $modulePath -output $infoPath 2>&1
		$createExit=$LASTEXITCODE
		$createOutput | Set-Content -LiteralPath $createLog
		Check ($createExit -eq 0 -and (Test-Path -LiteralPath $infoPath -PathType Leaf)) "moduleinfotool could not load/create metadata for $label; see $createLog"
		$validateOutput=& $moduleInfoTool -validate -path $modulePath -infopath $infoPath 2>&1
		$validateExit=$LASTEXITCODE
		$validateOutput | Set-Content -LiteralPath $validateLog
		Check ($validateExit -eq 0) "moduleinfotool could not validate $label; see $validateLog"
	} finally { $env:PATH=$originalPath }
}
function Paths([string]$where) {
	$store=Join-Path $where 'Data/Libraries/Factory'
	return @{app=(Join-Path $where 'HarmonyContinuation');bundle=(Join-Path $where 'VST3/HarmonyContinuation.vst3');plugin=(Join-Path $where 'VST3/HarmonyContinuation.vst3/Contents/x86_64-win/HarmonyContinuation.vst3');store=$store;db=(Join-Path $store "$libraryVersion/factory.db");active=(Join-Path $store 'active.txt');personal=(Join-Path $where 'Data/User/user.db')}
}
function FingerprintTree([string]$directory) {
	if (!(Test-Path -LiteralPath $directory -PathType Container)) { return '<missing>' }
	$prefix=[IO.Path]::GetFullPath($directory).TrimEnd([IO.Path]::DirectorySeparatorChar)+[IO.Path]::DirectorySeparatorChar
	$parts=@(Get-ChildItem -LiteralPath $directory -Force -Recurse | Sort-Object FullName | ForEach-Object {
		$relative=$_.FullName.Substring($prefix.Length)
		if ($_.PSIsContainer) { 'D:'+$relative }
		else { 'F:'+$relative+':'+(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
	})
	return [string]::Join("`n",$parts)
}
function FingerprintFile([string]$path) {
	if (!(Test-Path -LiteralPath $path -PathType Leaf)) { return '<missing>' }
	return (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
}
function HasFiles([string]$directory) {
	if (!(Test-Path -LiteralPath $directory -PathType Container)) { return $false }
	return (@(Get-ChildItem -LiteralPath $directory -Force -File -Recurse).Count -gt 0)
}
function AddRegistrySnapshotRows([Microsoft.Win32.RegistryKey]$key,[string]$keyRelative,[System.Collections.Generic.List[string]]$rows) {
	$rows.Add((ConvertTo-Json -InputObject ([ordered]@{key=$keyRelative;kind='Key'}) -Compress -Depth 10))
	foreach ($valueName in @($key.GetValueNames() | Sort-Object)) {
		$kind=$key.GetValueKind($valueName).ToString()
		$value=$key.GetValue($valueName,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
		if ($value -is [byte[]]) { $value=[BitConverter]::ToString($value).Replace('-','') }
		$rows.Add((ConvertTo-Json -InputObject ([ordered]@{key=$keyRelative;name=$valueName;kind=$kind;value=$value}) -Compress -Depth 10))
	}
	foreach ($childName in @($key.GetSubKeyNames() | Sort-Object)) {
		$child=$key.OpenSubKey($childName,$false)
		if ($null -ne $child) {
			try { AddRegistrySnapshotRows $child ($keyRelative+'\'+$childName) $rows }
			finally { $child.Dispose() }
		}
	}
}
function FingerprintRegistry([string]$keyPath) {
	$relative=$keyPath -replace '^HKCU:[\\/]+',''
	$relative=$relative.Replace('/','\')
	$key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($relative,$false)
	if ($null -eq $key) { return '<missing>' }
	$rows=[System.Collections.Generic.List[string]]::new()
	try { AddRegistrySnapshotRows $key $relative $rows }
	finally { $key.Dispose() }
	return [string]::Join("`n",$rows.ToArray())
}
function FingerprintRegistrations {
	return "application`n$(FingerprintRegistry $reg)`nlegacy-factory`n$(FingerprintRegistry $oldReg)"
}
function CaptureManagedState([hashtable]$paths,[string]$historyPath) {
	return @{
		app=(FingerprintTree $paths.app)
		registration=(FingerprintRegistrations)
		bundle=(FingerprintTree $paths.bundle)
		factoryTree=(FingerprintTree $paths.store)
		factoryDb=(FingerprintFile $paths.db)
		active=(FingerprintFile $paths.active)
		history=(FingerprintFile $historyPath)
		user=(FingerprintFile $paths.personal)
	}
}
function CompareManagedState([hashtable]$before,[hashtable]$after) {
	$changed=@()
	foreach ($name in $before.Keys) { if ($before[$name] -cne $after[$name]) { $changed+=$name } }
	return $changed
}
function AssertTransactionRollback([string]$name,[hashtable]$paths,[string]$historyPath,[string]$setupExe,[string]$testRoot,[string[]]$extra,[string]$requiredCause,[int]$expectedExit=-1,[string]$stateFile='') {
	$evidenceRoot=Join-Path $testRoot 'test-evidence'
	New-Item -ItemType Directory -Force -Path $evidenceRoot | Out-Null
	$stateBackup=Join-Path $evidenceRoot ($name+'-components.ini')
	if ($stateFile -and !(Test-Path -LiteralPath $stateFile -PathType Leaf)) { throw "Missing component-state fixture before $name." }
	if (Test-Path -LiteralPath $stateBackup) { throw "Rollback fixture already exists: $stateBackup" }
	$before=CaptureManagedState $paths $historyPath
	$fixtureHash=if($stateFile){FingerprintFile $stateFile}else{''}
	if ($stateFile) {
		Move-Item -LiteralPath $stateFile -Destination $stateBackup
		New-Item -ItemType Directory -Path $stateFile | Out-Null
	}
	$log=Join-Path $testRoot ($name+'.log')
	$exit=-1
	try { $exit=Run $setupExe $log (@('/COMPONENTS=plugin,library')+$extra) $null }
	finally {
		if ($stateFile -and (Test-Path -LiteralPath $stateBackup -PathType Leaf)) {
			if (Test-Path -LiteralPath $stateFile -PathType Container) { [IO.Directory]::Delete($stateFile,$false) }
			elseif (Test-Path -LiteralPath $stateFile -PathType Leaf) { Remove-Item -LiteralPath $stateFile -Force }
			Move-Item -LiteralPath $stateBackup -Destination $stateFile
		}
	}
	$after=CaptureManagedState $paths $historyPath
	$changes=@(CompareManagedState $before $after)
	$logText=if (Test-Path -LiteralPath $log -PathType Leaf) { Get-Content -LiteralPath $log -Raw } else { '' }
	$transactionFailed=$logText -match 'HC transaction failed'
	$rollbackVerified=$logText -match 'HC rollback verified'
	$causeLogged=(!$requiredCause -or $logText -match [regex]::Escape($requiredCause))
	$exitMatches=if($expectedExit -ge 0){$exit -eq $expectedExit}else{$exit -ne 0}
	ConvertTo-Json -InputObject $before -Depth 4 | Set-Content -LiteralPath (Join-Path $evidenceRoot ($name+'-before.json'))
	ConvertTo-Json -InputObject $after -Depth 4 | Set-Content -LiteralPath (Join-Path $evidenceRoot ($name+'-after.json'))
	$details=@("case=$name","exit=$exit","transactionFailed=$transactionFailed","rollbackVerified=$rollbackVerified","causeLogged=$causeLogged","stateChanges=$($changes -join ', ')")
	$details | Set-Content -LiteralPath (Join-Path $evidenceRoot ($name+'-state.txt'))
	Check ($transactionFailed -and $rollbackVerified -and $causeLogged -and $exitMatches -and $changes.Count -eq 0) ("Transaction rollback mismatch for $name`: exit=$exit expected=$(if($expectedExit -ge 0){$expectedExit}else{'nonzero'}), failure=$transactionFailed, rollback=$rollbackVerified, cause=$causeLogged, changed=$($changes -join '; '); log=$log")
	if ($stateFile -and (FingerprintFile $stateFile) -cne $fixtureHash) { throw "Restored components.ini fixture differs after $name." }
}
$fresh=Join-Path $root 'fresh';$setup=Compile $fresh;$p=Paths $fresh
New-Item -ItemType Directory -Force (Split-Path $p.personal) | Out-Null
[IO.File]::WriteAllText($p.personal,'User collection, favourites and preferences: preserve')
$userHash=(Get-FileHash $p.personal).Hash
$freshRegistrationBefore=FingerprintRegistrations
Check (!(Test-Path $p.app) -and !(Test-Path $p.bundle) -and !(Test-Path $p.plugin) -and !(Test-Path $p.db) -and !(Test-Path $p.active) -and !(HasFiles $p.store) -and $freshRegistrationBefore -eq "application`n<missing>`nlegacy-factory`n<missing>") 'Fresh install rollback precondition is not empty'
$freshRollbackLogPath=Join-Path $fresh 'fresh-after-registration-rollback.log'
$freshRollbackExit=Run $setup $freshRollbackLogPath @('/COMPONENTS=plugin,library','/HCFAIL=after-registration') $null
$freshRollbackLog=Get-Content -LiteralPath $freshRollbackLogPath -Raw
$freshRollbackClean=!(Test-Path $p.app) -and !(Test-Path $reg) -and !(Test-Path $oldReg) -and !(Test-Path $p.plugin) -and !(Test-Path $p.db) -and !(Test-Path $p.active) -and !(HasFiles $p.bundle) -and !(HasFiles $p.store) -and (FingerprintRegistrations) -ceq $freshRegistrationBefore -and (Get-FileHash $p.personal).Hash -eq $userHash
Check ($freshRollbackExit -eq 201 -and $freshRollbackLog -match 'HC transaction failed' -and $freshRollbackLog -match 'HC rollback verified' -and $freshRollbackLog -match 'after-registration' -and $freshRollbackClean) 'Fresh late-failure rollback left registration, app, plugin, Factory or user changes'
Run $setup (Join-Path $fresh 'install-retry.log')
Check ((Test-Path $p.plugin) -and (Test-Path $p.db) -and (Test-Path $reg) -and (Get-FileHash $p.personal).Hash -eq $userHash) 'Fresh install retry missing component, registration or user data'
Check (!(Test-Path (Join-Path (Split-Path (Split-Path $p.plugin)) 'Resources/factory.db'))) 'Hidden Factory remains in VST3 component'
$pluginHash=(Get-FileHash $p.plugin).Hash
$foreign=Join-Path $fresh 'VST3/OtherPlugin.keep';[IO.File]::WriteAllText($foreign,'Unrelated VST3 must survive')
$inside=Join-Path $fresh 'VST3/HarmonyContinuation.vst3/personal.keep';[IO.File]::WriteAllText($inside,'Unmanaged file must survive')
Run $setup (Join-Path $fresh 'remove-library.log') @('/OPERATION=modify','/REMOVE=library','/COMPONENTS=""')
Check (!(Test-Path $p.db) -and (Get-FileHash $p.plugin).Hash -eq $pluginHash) 'Removing Factory affected plugin or failed'
Run $setup (Join-Path $fresh 'add-library.log') @('/OPERATION=modify','/COMPONENTS=library')
Check ((Test-Path $p.db) -and (Get-FileHash $p.plugin).Hash -eq $pluginHash) 'Adding Factory affected plugin'
[IO.File]::WriteAllText($p.db,'damaged official Factory fixture')
Run $setup (Join-Path $fresh 'repair-library.log') @('/OPERATION=repair','/COMPONENTS=library')
Check ((Get-FileHash $p.db).Hash -eq $factoryHash -and (Get-FileHash $p.plugin).Hash -eq $pluginHash) 'Repair did not restore only the managed Factory'
Run $setup (Join-Path $fresh 'remove-plugin.log') @('/OPERATION=modify','/REMOVE=plugin','/COMPONENTS=""')
Check (!(Test-Path $p.plugin) -and (Test-Path $p.db)) 'Removing VST3 affected Factory or failed'
Run $setup (Join-Path $fresh 'add-plugin.log') @('/OPERATION=modify','/COMPONENTS=plugin')
Check ((Get-FileHash $p.plugin).Hash -eq $pluginHash -and (Get-FileHash $p.db).Hash -eq $factoryHash) 'Adding VST3 changed Factory'
Run (Join-Path $p.app 'unins000.exe') (Join-Path $fresh 'uninstall.log')
Check (!(Test-Path $p.plugin) -and !(Test-Path $p.db) -and !(Test-Path $reg)) 'Full uninstall left managed components or registration'
Check ((Get-FileHash $p.personal).Hash -eq $userHash -and (Test-Path $foreign) -and (Test-Path $inside)) 'Uninstall touched personal/unmanaged data'
"Fresh and maintenance checks passed: $checks" | Set-Content (Join-Path $root 'fresh-result.txt')
if ($PreviousPackageDirectory) {
 $migration=Join-Path $root 'upgrade';$p=Paths $migration
 $oldSetup=Compile $migration $false $false $true
 New-Item -ItemType Directory -Force (Split-Path $p.personal),(Join-Path $migration 'Data/Libraries/Factory/4') | Out-Null
 [IO.File]::WriteAllText($p.personal,'Existing user collection unchanged')
 $userHash=(Get-FileHash $p.personal).Hash
 $history=Join-Path $migration 'Data/Libraries/Factory/4/factory.db'
	$previousHistory=Join-Path $previousStage 'official-factory/factory-v4.db'
	if (!(Test-Path -LiteralPath $previousHistory)) { throw 'Formal previous payload is missing its V4 history fixture.' }
	Copy-Item -LiteralPath $previousHistory -Destination $history
 $historyHash=(Get-FileHash $history).Hash
 Run $oldSetup (Join-Path $migration 'previous-install.log')
	$previousRegistrationVersion=(Get-ItemProperty -LiteralPath $reg -Name DisplayVersion).DisplayVersion
	Check ($previousRegistrationVersion -eq $PreviousVersion) 'Formal previous install registration version mismatch'
 $oldPluginHash=(Get-FileHash $p.plugin).Hash
	$oldModuleInfo=Get-Content (Join-Path $p.bundle 'Contents/Resources/moduleinfo.json') -Raw
	Check ($oldModuleInfo -match '"Version":\s*"0\.10\.0"') 'Formal previous install did not expose the 0.10.0 module version'
 $setup=Compile $migration
	$stateFile=Join-Path $p.app 'components.ini'
	# This original file/directory collision runs against the formal install before the legal library-only update.
	AssertTransactionRollback -name 'component-state-rollback' -paths $p -historyPath $history -setupExe $setup -testRoot $migration -extra @() -requiredCause 'Cannot record component state' -stateFile $stateFile
	Run $setup (Join-Path $migration 'library-only-update.log') @('/OPERATION=modify','/COMPONENTS=library')
	Check ((Get-FileHash $p.plugin).Hash -eq $oldPluginHash) 'Library-only upgrade changed the old plugin'
	$developmentRegistrationVersion=(Get-ItemProperty -LiteralPath $reg -Name DisplayVersion).DisplayVersion
	Check ($developmentRegistrationVersion -eq $version) 'Library-only upgrade did not update the isolated management version'
	$activeFile=$p.active
	AssertTransactionRollback -name 'after-factory-rollback' -paths $p -historyPath $history -setupExe $setup -testRoot $migration -extra @('/HCFAIL=after-factory') -requiredCause 'after-factory'
	AssertTransactionRollback -name 'after-manifest-rollback' -paths $p -historyPath $history -setupExe $setup -testRoot $migration -extra @('/HCFAIL=after-manifest') -requiredCause 'after-manifest'
	AssertTransactionRollback -name 'after-registration-rollback' -paths $p -historyPath $history -setupExe $setup -testRoot $migration -extra @('/HCFAIL=after-registration') -requiredCause 'after-registration' -expectedExit 201
	ValidateInstalledModule 'formal-after-rollback' $p.bundle $PreviousVersion $p.app
	 Run $setup (Join-Path $migration 'plugin-update.log') @('/COMPONENTS=plugin,library')
	$expectedPlugin=Join-Path $stage 'HarmonyContinuation.vst3/Contents/x86_64-win/HarmonyContinuation.vst3'
	Check ((Get-FileHash $p.plugin).Hash -eq (Get-FileHash $expectedPlugin).Hash) 'Plugin upgrade identity mismatch'
 Check ((Get-FileHash $p.db).Hash -eq $factoryHash -and (Get-FileHash $history).Hash -eq $historyHash -and (Get-FileHash $p.personal).Hash -eq $userHash) 'Upgrade changed Factory history or user data'
	ValidateInstalledModule 'development-after-upgrade' $p.bundle $version $p.app
	$expectedPluginHash=(Get-FileHash $expectedPlugin).Hash
	# Exercise repair against damaged managed files, then verify both installed bytes and user/history protection.
	[IO.File]::WriteAllText($p.plugin,'damaged isolated plugin fixture')
	[IO.File]::WriteAllText($p.db,'damaged isolated Factory fixture')
	Run $setup (Join-Path $migration 'development-repair.log') @('/OPERATION=repair','/COMPONENTS=plugin,library')
	Check ((FingerprintFile $p.plugin) -eq $expectedPluginHash -and (FingerprintFile $p.db) -eq $factoryHash -and (FingerprintFile $history) -eq $historyHash -and (FingerprintFile $p.personal) -eq $userHash) 'Development repair did not restore plugin/Factory or changed personal/history data'
	ValidateInstalledModule 'development-after-repair' $p.bundle $version $p.app
	$downgradeBefore=CaptureManagedState $p $history
	$downgradeExit=Run $oldSetup (Join-Path $migration 'downgrade-attempt.log') @('/COMPONENTS=plugin') $null
	$downgradeLog=Get-Content (Join-Path $migration 'downgrade-attempt.log') -Raw
	$currentPluginHash=(Get-FileHash $p.plugin).Hash
	$oldPayloadPlugin=Join-Path $previousStage 'HarmonyContinuation.vst3/Contents/x86_64-win/HarmonyContinuation.vst3'
	$oldPayloadPluginHash=(Get-FileHash $oldPayloadPlugin).Hash
	if ($currentPluginHash -eq $oldPayloadPluginHash -and (Get-ItemProperty -LiteralPath $reg -Name DisplayVersion).DisplayVersion -eq $PreviousVersion) {
		Check ($downgradeExit -eq 0 -and (FingerprintFile $p.db) -eq $previousFactoryHash -and (FingerprintFile $history) -eq $historyHash -and (FingerprintFile $p.personal) -eq $userHash) 'Direct downgrade did not complete cleanly or changed Factory history/user data'
		ValidateInstalledModule 'formal-after-direct-downgrade' $p.bundle $PreviousVersion $p.app
		$downgradeResult='direct downgrade accepted'
	} else {
		$explicitRefusal=($downgradeLog -match '(?i)Downgrade cancelled|A newer plugin is installed|newer plugin|较新主程序|取消降级')
		$downgradeAfter=CaptureManagedState $p $history
		$downgradeChanges=@(CompareManagedState $downgradeBefore $downgradeAfter)
		Check ($explicitRefusal -and $downgradeChanges.Count -eq 0) ("Downgrade was neither applied nor explicitly rejected without changing data; changed=$($downgradeChanges -join ', ')")
		$downgradeResult='direct downgrade explicitly refused; uninstall/reinstall rollback verified'
		Run (Join-Path $p.app 'unins000.exe') (Join-Path $migration 'uninstall-dev-before-rollback.log')
		Check (!(Test-Path $reg) -and !(Test-Path $oldReg) -and !(Test-Path $p.plugin) -and !(Test-Path $p.db) -and !(HasFiles $p.app)) 'Removing the isolated development install left managed components/support files/registration'
		Check ((Get-FileHash $history).Hash -eq $historyHash -and (Get-FileHash $p.personal).Hash -eq $userHash) 'Removing the isolated development install damaged personal or historical data'
		Run $oldSetup (Join-Path $migration 'rollback-formal-install.log')
		Check ((Get-FileHash $p.plugin).Hash -eq $oldPayloadPluginHash -and (Get-FileHash $p.db).Hash -eq $previousFactoryHash -and (Get-FileHash $history).Hash -eq $historyHash -and (Get-FileHash $p.personal).Hash -eq $userHash -and (Test-Path $reg)) 'Uninstall/reinstall rollback did not restore formal plugin while preserving personal/history data'
		ValidateInstalledModule 'formal-after-safe-downgrade' $p.bundle $PreviousVersion $p.app
	}
	Check ((Get-FileHash $p.plugin).Hash -eq $oldPayloadPluginHash -and (Get-FileHash $p.db).Hash -eq $previousFactoryHash -and (Get-FileHash $history).Hash -eq $historyHash -and (Get-FileHash $p.personal).Hash -eq $userHash -and (Get-ItemProperty -LiteralPath $reg -Name DisplayVersion).DisplayVersion -eq $PreviousVersion) 'Rollback state does not match formal v0.10.0 and preserved data'
	ValidateInstalledModule 'formal-before-final-uninstall' $p.bundle $PreviousVersion $p.app
	Run (Join-Path $p.app 'unins000.exe') (Join-Path $migration 'uninstall.log')
	Check (!(Test-Path $reg) -and !(Test-Path $oldReg) -and !(Test-Path $p.plugin) -and !(Test-Path $p.db) -and !(HasFiles $p.app)) 'Upgraded uninstall left managed components/support files/registration'
 Check ((Get-FileHash $history).Hash -eq $historyHash -and (Get-FileHash $p.personal).Hash -eq $userHash) 'Upgraded uninstall damaged historical Factory or user data'
	$emptyRegistration="application`n<missing>`nlegacy-factory`n<missing>"
	Check ((FingerprintRegistrations) -ceq $emptyRegistration) 'Uninstall left an isolated uninstall registration value/subkey'
	# Leave a separately installed, fully isolated development copy for the parent to initialize and query.
	$verification=Join-Path $root 'verification-install';$vp=Paths $verification
	New-Item -ItemType Directory -Force -Path (Split-Path $vp.personal) | Out-Null
	[IO.File]::WriteAllText($vp.personal,'Verification-only user fixture; not a production database')
	$verificationUserHash=(Get-FileHash $vp.personal).Hash
	$verificationSetup=Compile $verification
	Run $verificationSetup (Join-Path $verification 'verification-install.log')
	$verificationPluginHash=(Get-FileHash $vp.plugin).Hash
	Check ($verificationPluginHash -eq $expectedPluginHash -and (FingerprintFile $vp.db) -eq $factoryHash -and (Test-Path $reg) -and (Get-FileHash $vp.personal).Hash -eq $verificationUserHash) 'Retained verification install identity or isolation fixture mismatch'
	ValidateInstalledModule 'retained-development-verification' $vp.bundle $version $vp.app
	@("TestRoot=$verification","App=$($vp.app)","PluginBundle=$($vp.bundle)","PluginModule=$($vp.plugin)","FactoryDatabase=$($vp.db)","UserFixture=$($vp.personal)","UserFixtureSHA256=$verificationUserHash") | Set-Content -LiteralPath (Join-Path $root 'verification-paths.txt')
	"Unified isolated lifecycle $checks checks PASS; fresh/modify/repair/remove, $PreviousVersion to $version upgrade, transaction rollback, old/new module load, $downgradeResult, uninstall, preserved user/V4 history; retained verification install at $verification" | Tee-Object (Join-Path $root 'result.txt')
 return
}
$migration=Join-Path $root 'migration';$p=Paths $migration
$legacyFull=Compile $migration $true $false;$legacyLibrary=Compile $migration $true $true;$setup=Compile $migration
New-Item -ItemType Directory -Force (Split-Path $p.personal),(Join-Path $migration 'Data/Libraries/Factory/2') | Out-Null
[IO.File]::WriteAllText($p.personal,'Legacy user collection unchanged')
$userHash=(Get-FileHash $p.personal).Hash
Copy-Item -LiteralPath (Join-Path $repo "$BuildDirectory/factory-v2.db") -Destination (Join-Path $migration 'Data/Libraries/Factory/2/factory.db')
Run $legacyFull (Join-Path $migration 'legacy-main.log')
Run $legacyLibrary (Join-Path $migration 'legacy-library.log')
Check ((Test-Path $reg) -and (Test-Path $oldReg)) 'Dual legacy registrations not created'
$oldPluginHash=(Get-FileHash $p.plugin).Hash
Run $setup (Join-Path $migration 'library-only-migration.log') @('/COMPONENTS=library')
Check ((Test-Path $reg) -and !(Test-Path $oldReg) -and (Get-FileHash $p.plugin).Hash -eq $oldPluginHash) 'Legacy migration touched unselected plugin or left dual records'
$libraryHash=(Get-FileHash $p.db).Hash
Run $setup (Join-Path $migration 'plugin-only-update.log') @('/COMPONENTS=plugin')
Check ((Get-FileHash $p.plugin).Hash -eq (Get-FileHash (Join-Path $stage 'HarmonyContinuation.vst3/Contents/x86_64-win/HarmonyContinuation.vst3')).Hash -and (Get-FileHash $p.db).Hash -eq $libraryHash) 'Plugin-only update changed Library'
$lock=[IO.File]::Open($p.plugin,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::None)
try { Run $setup (Join-Path $migration 'locked-update.log') @('/COMPONENTS=plugin') $false } finally { $lock.Dispose() }
Check ((Get-FileHash $p.personal).Hash -eq $userHash) 'Migration changed User Library'
Run (Join-Path $p.app 'unins000.exe') (Join-Path $migration 'uninstall.log')
Check (!(Test-Path $reg) -and !(Test-Path $oldReg) -and !(Test-Path $p.plugin) -and !(Test-Path $p.db)) 'Migrated uninstall left registered/managed components'
Check ((Get-FileHash $p.personal).Hash -eq $userHash -and (Test-Path (Join-Path $migration 'Data/Libraries/Factory/2/factory.db'))) 'Historical Factory or personal collection lost'
"Unified isolated lifecycle $checks checks PASS; fresh, dual legacy migration, independent update/remove/repair, lock rejection, uninstall and preserved personal/history/unmanaged files" | Tee-Object (Join-Path $root 'result.txt')
