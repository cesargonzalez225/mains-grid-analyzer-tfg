@echo off
g++ -std=c++17 -Wall -Wextra -DFREEGLUT_STATIC -I../analysis ^
    main.cpp App.cpp TextRenderer.cpp SerialPort.cpp SerialReaderThread.cpp DashboardState.cpp ProtocolParser.cpp CsvLogger.cpp ^
    ../analysis/IticClassifier.cpp ^
    -o dashboard.exe ^
    -lfreeglut -lopengl32 -lglu32 -lwinmm -lgdi32 -luser32 ^
    -static -static-libgcc -static-libstdc++
