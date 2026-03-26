$ErrorActionPreference = 'Stop'

function Ensure-Winget {
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
        throw 'winget is required. Install App Installer from Microsoft Store.'
    }
}

Ensure-Winget

$packages = @(
    @{ Id = 'Kitware.CMake'; Name = 'CMake' },
    @{ Id = 'Git.Git'; Name = 'Git' },
    @{ Id = 'Ninja-build.Ninja'; Name = 'Ninja' },
    @{ Id = 'Microsoft.VisualStudio.2022.BuildTools'; Name = 'VS 2022 Build Tools' }
)

foreach ($pkg in $packages) {
    Write-Host "Installing $($pkg.Name)..."
    winget install --id $pkg.Id --exact --accept-package-agreements --accept-source-agreements
}

Write-Host 'Bootstrap complete. Ensure MSVC C++ workload is installed for Visual Studio Build Tools.'
