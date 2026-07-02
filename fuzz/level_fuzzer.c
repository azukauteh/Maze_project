int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    if (size < 2) return 0;



    uint8_t route = data[0] % 4;

    const uint8_t *payload = data + 1;

    size_t psize = size - 1;



    switch (route) {

        case 0: {

            /* text map -> validate -> pathfind */

            MazeMap map;

            if (parse_maze_map(payload, psize, &map)) {

                MapValidation v = validate_maze_map(&map);

                if (v.valid) {

                    GameState s;

                    memset(&s, 0, sizeof(s));

                    s.width = 320; s.height = 240;

                    /* copy parsed map into engine state */

                    for (int i = 0; i < MAP_WIDTH * MAP_HEIGHT; i++)

                        s.map[i] = (map.cells[i/MAP_WIDTH][i%MAP_WIDTH] == '#') ? 1

                                 : (map.cells[i/MAP_WIDTH][i%MAP_WIDTH] == 'E') ? 2 : 0;

                    pf_solve_from_player(&s);

                }

            }

            break;

        }

        case 1: {

            /* binary map -> serialise -> reload */

            MazeMap m1, m2;

            if (parse_binary_map(payload, psize, &m1)) {

                uint8_t bin[8 + MAP_MAX_WIDTH * MAP_MAX_HEIGHT];

                size_t wr = 0;

                if (mazemap_to_binary(&m1, bin, sizeof(bin), &wr))

                    parse_binary_map(bin, wr, &m2);

            }

            break;

        }

        case 2: {

            /* .maz level file -> load -> validate -> pathfind */

            LoadedLevel level;

            if (level_load_from_data((const char *)payload, psize, &level)) {

                level_validate(&level);

                GameState s;

                memset(&s, 0, sizeof(s));

                s.width = 320; s.height = 240;

                memcpy(s.map, level.map.cells, sizeof(s.map));

                pf_solve_from_player(&s);

            }

            break;

        }

        case 3: {

            /* config -> get typed values -> feed to savegame */

            Config cfg;

            if (config_parse((const char *)payload, psize, &cfg) > 0) {

                config_get_int  (&cfg, "spawn", "x",     0);

                config_get_float(&cfg, "spawn", "angle", 0);

                config_get      (&cfg, "meta",  "name");

            }

            break;

        }

    }

    return 0;

}
