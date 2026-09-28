$ErrorActionPreference = "Stop"

# Keep this test file ASCII-only so Windows PowerShell 5 reads it correctly without a BOM.
Push-Location $PSScriptRoot
try {
    & g++ -std=c++17 -Wall -Wextra -pedantic main.cpp -o train_system.exe
    if ($LASTEXITCODE -ne 0) {
        throw "Compilation failed with exit code $LASTEXITCODE"
    }

    function Invoke-TrainSystem([string[]] $Lines) {
        $inputText = ($Lines -join "`n") + "`n"
        $result = $inputText | & .\train_system.exe 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) {
            throw "Program exited abnormally with code $LASTEXITCODE`n$result"
        }
        return $result
    }

    function Assert-Contains([string] $Text, [string] $Expected, [string] $CaseName) {
        if (-not $Text.Contains($Expected)) {
            throw "[$CaseName] Missing expected text: $Expected"
        }
    }

    function Assert-Regex([string] $Text, [string] $Pattern, [string] $CaseName) {
        if (-not [regex]::IsMatch($Text, $Pattern)) {
            throw "[$CaseName] Missing expected pattern: $Pattern"
        }
    }

    function Assert-MatchCount([string] $Text, [string] $Pattern, [int] $Expected, [string] $CaseName) {
        $actual = ([regex]::Matches($Text, $Pattern)).Count
        if ($actual -ne $Expected) {
            throw "[$CaseName] Pattern '$Pattern' expected $Expected matches, got $actual"
        }
    }

    $case = "initial state, availability, and invalid lookup"
    $output = Invoke-TrainSystem @(
        '1',
        '2', '1', '6', '0',
        '2', '3', '2',
        '4', '0000',
        '0'
    )
    Assert-Contains $output 'G1001' $case
    Assert-Regex $output '37.*10.*27' $case
    Assert-MatchCount $output '01.*02.*03.*04.*05' 5 $case
    Assert-MatchCount $output '0000' 0 $case

    $case = "fares, balancing, segment reuse, duplicate ID suffix, and revenue"
    $output = Invoke-TrainSystem @(
        '3', '1', '3', '1', 'Alice', '1234',
        '3', '1', '3', '1', 'Bob',   '1234',
        '3', '3', '6', '1', 'Chen', '5678',
        '3', '1', '6', '2', 'Ding', '9X9X',
        '4', '1234',
        '7',
        '0'
    )
    Assert-Regex $output 'Alice.*1234.*1.*01.*400' $case
    Assert-Regex $output 'Bob.*1234.*2.*01.*400' $case
    Assert-Regex $output 'Chen.*5678.*1.*01.*550' $case
    Assert-Regex $output 'Ding.*9X9X.*3.*01.*750' $case
    Assert-MatchCount $output '1234' 4 $case
    Assert-Contains $output '2100' $case

    $case = "first-class capacity boundary and sold-out recommendation"
    $inputs = [System.Collections.Generic.List[string]]::new()
    for ($i = 1; $i -le 10; $i++) {
        $inputs.Add('3')
        $inputs.Add('1')
        $inputs.Add('6')
        $inputs.Add('1')
        $inputs.Add("P$i")
        $inputs.Add($i.ToString('0000'))
    }
    $inputs.Add('3')
    $inputs.Add('1')
    $inputs.Add('6')
    $inputs.Add('1')
    $inputs.Add('1')
    $inputs.Add('0')
    $output = Invoke-TrainSystem $inputs.ToArray()
    Assert-MatchCount $output '1000(0[1-9]|10)' 10 $case
    Assert-Contains $output 'G1003' $case
    Assert-Contains $output 'G1005' $case
    Assert-Contains $output 'G1007' $case
    Assert-Regex $output 'P9.*1.*05.*850' $case
    Assert-Regex $output 'P10.*2.*05.*850' $case
    Assert-Regex $output '27.*0.*27' $case

    $case = "boarding, alighting, deletion, seat restoration, and ledger retention"
    $output = Invoke-TrainSystem @(
        '3', '1', '3', '1', 'Alice', '1234',
        '5', '1',
        '4', '1234',
        '6', '3',
        '4', '1234',
        '2', '1', '3', '1',
        '7',
        '0'
    )
    Assert-MatchCount $output 'Alice.*1234' 4 $case
    Assert-MatchCount $output '1234' 4 $case
    Assert-MatchCount $output '01.*02.*03.*04.*05' 2 $case
    Assert-Contains $output '400' $case

    $case = "invalid ticket input"
    $output = Invoke-TrainSystem @(
        '3', '4', '2',
        '3', '1', '2', '2', '',
        '3', '1', '2', '2', 'Name', 'ABC',
        '0'
    )
    Assert-MatchCount $output '100001' 0 $case
    Assert-MatchCount $output 'Name' 0 $case

    Write-Host 'PASS: all 5 test groups completed successfully.'
}
finally {
    Pop-Location
}
