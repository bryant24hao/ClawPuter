// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "CardputerDesktopPet",
    platforms: [.macOS(.v13)],
    targets: [
        .executableTarget(
            name: "CardputerDesktopPet",
            path: "Sources",
            linkerSettings: [
                .unsafeFlags(["-Xlinker", "-rpath", "-Xlinker", "@executable_path/../Frameworks"])
            ]
        )
    ]
)
