Set GENERATE_PNGS and RENDER_ON_WINDOW either to true or false depending on whatever you want.

-If GENERATE_PNGS = true, the program will generate png images of each frame in a "frames" folder located in the same folder as your .exe (make sure to create the "frames" folder beforehand otherwise the program will complain at execution)

-If RENDER_ON_WINDOW = true, the program will open an SFML window on full screen mode. You can move around by dragging and zoom with your mousewheel. You can display the boxes needed for Barn-Hut approximation by pressing your spacebar. If the framerate is too slow for your liking, you can lower N or increase THETA (though the Barn-Hut approximation error may accumulate and become visible).
