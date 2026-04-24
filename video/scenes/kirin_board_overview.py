from __future__ import annotations

from manim import *


FONT = "JetBrains Mono"
BG = "#1A1B26"
SURFACE = "#24283B"
BOARD_DARK = "#414868"
BOARD_LIGHT = "#292E42"
INK = "#C0CAF5"
MUTED = "#565F89"
COPPER = "#E0AF68"
RAIL = "#7AA2F7"
SENSOR = "#7DCFFF"
ENGINE = "#9ECE6A"
GCODE = "#F7768E"
PURPLE = "#BB9AF7"


class KirinBoardOverview(Scene):
    """Storyboard for a short Kirin autonomous chess board overview video."""

    def construct(self) -> None:
        self.camera.background_color = BG
        hardware_board = self.title_scene()
        hardware_board = self.hardware_scene(hardware_board)
        self.sensor_scene(hardware_board)
        self.display_ui_scene()
        self.software_scene()
        self.path_planning_scene()
        self.capture_identity_scene()
        self.closing_scene()

    def title_scene(self) -> VGroup:
        title = self.text("Kirin Autonomous Chess", font_size=54, weight=BOLD)
        subtitle = self.text(
            "A physical board driven by a chess engine, sensors, and motion control",
            font_size=24,
        )

        board = self.make_chessboard(square_size=0.28)
        title_group = VGroup(title, subtitle, board).arrange(DOWN, buff=0.38).move_to(ORIGIN)
        hardware_board = self.make_board_with_storage()

        self.play(FadeIn(board, shift=UP * 0.2), Write(title), FadeIn(subtitle))
        self.wait(5.0)
        self.play(FadeOut(title), FadeOut(subtitle), ReplacementTransform(board, hardware_board), run_time=1.2)
        self.wait(0.5)
        return hardware_board

    def hardware_scene(self, board: VGroup | None = None) -> VGroup:
        heading = self.heading("Hardware Layer")
        should_create_board = board is None
        if board is None:
            board = self.make_board_with_storage()
        board_squares = board[0]
        gantry_start = board_squares[1 * 8 + 0].get_center()
        pickup = board_squares[1 * 8 + 1].get_center()
        x_stop = board_squares[1 * 8 + 5].get_center()
        y_stop = board_squares[4 * 8 + 5].get_center()
        end = board_squares[4 * 8 + 2].get_center()

        gantry = self.make_gantry(gantry_start)
        piece = Circle(radius=0.105, color=SENSOR, fill_color=SENSOR, fill_opacity=1, stroke_color=INK, stroke_width=2)
        piece.move_to(pickup).set_z_index(5)
        labels = VGroup(
            self.callout("12 in. board", board[0], UP),
            self.callout("Storage", board[1], DOWN),
            self.callout("Storage", board[2], DOWN),
        )
        carriage_and_piece = VGroup(gantry[2], piece)
        y_axis = VGroup(gantry[1], gantry[2], piece)
        motion_trace = VGroup(
            Line(gantry_start, pickup, color=MUTED, stroke_width=4),
            Line(pickup, x_stop, color=COPPER, stroke_width=5),
            Line(x_stop, y_stop, color=COPPER, stroke_width=5),
            Line(y_stop, end, color=COPPER, stroke_width=5),
        ).set_opacity(0.7)

        self.play(FadeIn(heading, shift=DOWN))
        if should_create_board:
            self.play(Create(board), run_time=1.6)
        self.play(GrowFromCenter(piece), run_time=0.5)
        self.play(LaggedStart(*(FadeIn(label, shift=UP * 0.15) for label in labels), lag_ratio=0.15))
        self.wait(3.0)
        self.play(FadeOut(labels), run_time=0.6)
        self.play(Create(gantry), run_time=1.2)
        self.play(gantry[2].animate.shift(pickup - gantry_start), Create(motion_trace[0]), run_time=0.9)
        self.play(
            gantry[2][1].animate.set_fill(COPPER).set_color(COPPER),
            piece.animate.scale(0.85).set_fill(SENSOR).set_color(SENSOR).set_z_index(5),
            run_time=0.45,
        )
        self.play(carriage_and_piece.animate.shift(x_stop - pickup), Create(motion_trace[1]), run_time=1.8)
        self.play(y_axis.animate.shift(y_stop - x_stop), Create(motion_trace[2]), run_time=1.8)
        self.play(carriage_and_piece.animate.shift(end - y_stop), Create(motion_trace[3]), run_time=1.8)
        self.play(
            gantry[2][1].animate.set_fill(INK).set_color(INK),
            piece.animate.scale(1 / 0.85).set_fill(SENSOR).set_color(SENSOR).set_z_index(5),
            run_time=0.45,
        )
        self.play(VGroup(gantry[1], gantry[2]).animate.shift(UP * 0.9), run_time=0.9)
        self.wait(2.0)
        self.play(FadeOut(VGroup(heading, gantry, piece, motion_trace)))
        return board

    def sensor_scene(self, board: VGroup | None = None) -> None:
        heading = self.heading("Sensing Layer")
        should_create_board = board is None
        if board is None:
            board = self.make_board_with_storage()

        board_squares = board[0]
        left_storage = board[1]
        right_storage = board[2]

        muxes = {
            0: self.mux_box("Mux 0", "left storage").next_to(left_storage, LEFT, buff=0.45),
            1: self.mux_box("Mux 1", "files a-b").shift(DOWN * 2.45 + LEFT * 2.25),
            2: self.mux_box("Mux 2", "files c-d").shift(DOWN * 2.45 + LEFT * 0.75),
            3: self.mux_box("Mux 3", "files e-f").shift(DOWN * 2.45 + RIGHT * 0.75),
            4: self.mux_box("Mux 4", "files g-h").shift(DOWN * 2.45 + RIGHT * 2.25),
            5: self.mux_box("Mux 5", "right storage").next_to(right_storage, RIGHT, buff=0.45),
        }
        mux_group = VGroup(*muxes.values())
        sensors = VGroup()
        lines = VGroup()
        channel_notes = VGroup(
            self.text("Storage muxes: channels 0-7 = left file, 8-15 = right file", font_size=16),
            self.text("Board muxes: each mux reads two files, top-to-bottom within each file", font_size=16),
        ).arrange(DOWN, buff=0.16).to_edge(DOWN, buff=0.24)

        def sensor_for(square: Mobject, mux_index: int) -> None:
            dot = Dot(square.get_center(), radius=0.032, color=SENSOR).set_z_index(4)
            line_end = muxes[mux_index].get_left() if muxes[mux_index].get_x() > dot.get_x() else muxes[mux_index].get_right()
            line = Line(dot.get_center(), line_end, color=SENSOR, stroke_width=1.1, stroke_opacity=0.3)
            sensors.add(dot)
            lines.add(line)

        for storage_zone, mux_index in [(left_storage, 0), (right_storage, 5)]:
            for file in range(2):
                for rank in reversed(range(8)):
                    sensor_for(storage_zone[rank * 2 + file], mux_index)

        for mux_index, file_start in enumerate(range(0, 8, 2), start=1):
            for file in [file_start, file_start + 1]:
                for rank in reversed(range(8)):
                    sensor_for(board_squares[rank * 8 + file], mux_index)

        self.play(FadeIn(heading))
        if should_create_board:
            self.play(FadeIn(board), run_time=0.8)
        self.play(LaggedStartMap(GrowFromCenter, sensors, lag_ratio=0.003), run_time=1.8)
        self.play(FadeIn(mux_group, shift=UP * 0.15), FadeIn(channel_notes), run_time=0.8)
        self.play(LaggedStart(*(Create(line) for line in lines), lag_ratio=0.002), run_time=2.4)
        self.wait(8.0)
        self.play(FadeOut(VGroup(heading, board, sensors, mux_group, channel_notes, lines)))

    def display_ui_scene(self) -> None:
        heading = self.heading("Display + Operator UI")

        oled_frame = RoundedRectangle(
            corner_radius=0.16,
            width=4.4,
            height=2.25,
            color=RAIL,
            fill_color=SURFACE,
            fill_opacity=1,
            stroke_width=3,
        ).shift(LEFT * 1.7 + UP * 0.25)
        oled_screen = RoundedRectangle(
            corner_radius=0.08,
            width=3.7,
            height=1.55,
            color=SENSOR,
            fill_color=BG,
            fill_opacity=1,
            stroke_width=2,
        ).move_to(oled_frame)
        oled_text = VGroup(
            self.text("KIRIN STATUS", font_size=20, color=SENSOR, weight=BOLD),
            self.text("White to move", font_size=17),
            self.text("Scanner: stable", font_size=15, color=ENGINE),
            self.text("Gantry: idle", font_size=15, color=COPPER),
        ).arrange(DOWN, buff=0.09).move_to(oled_screen)
        oled = VGroup(oled_frame, oled_screen, oled_text)

        buttons = VGroup(
            self.ui_button("START", ENGINE),
            self.ui_button("STOP", GCODE),
            self.ui_button("RESET", PURPLE),
        ).arrange(DOWN, buff=0.36).shift(RIGHT * 2.35 + UP * 0.2)

        i2c_label = self.text("128x64 I2C OLED", font_size=22, color=SENSOR).next_to(oled, DOWN, buff=0.32)
        button_label = self.text("Physical controls", font_size=22, color=INK).next_to(buttons, DOWN, buff=0.32)
        notes = VGroup(
            self.text("Status feedback for scan, engine, and motion state", font_size=20),
            self.text("Start / stop / reset keep physical mode operator-safe", font_size=20),
        ).arrange(DOWN, buff=0.2).to_edge(DOWN, buff=0.4)

        self.play(FadeIn(heading))
        self.play(Create(oled_frame), FadeIn(oled_screen), run_time=1.0)
        self.play(LaggedStart(*(FadeIn(line, shift=RIGHT * 0.08) for line in oled_text), lag_ratio=0.18))
        self.play(FadeIn(i2c_label), LaggedStart(*(GrowFromCenter(button) for button in buttons), lag_ratio=0.2))
        self.play(FadeIn(button_label), FadeIn(notes, shift=UP * 0.15))
        self.play(
            buttons[0][0].animate.set_fill(ENGINE, opacity=0.9),
            buttons[1][0].animate.set_fill(GCODE, opacity=0.9),
            buttons[2][0].animate.set_fill(PURPLE, opacity=0.9),
            run_time=0.7,
        )
        self.wait(7.0)
        self.play(FadeOut(VGroup(heading, oled, buttons, i2c_label, button_label, notes)))

    def software_scene(self) -> None:
        heading = self.heading("Software Layer")
        stages = [
            (
                "Game Controller",
                "main orchestration",
                PURPLE,
                ["Coordinates game state and mode", "Routes engine moves to hardware", "Handles physical-mode safety flow"],
            ),
            (
                "Engine",
                "move selection",
                ENGINE,
                ["Bitboards for compact board state", "Move generation + legality checks", "Alpha-beta search with difficulty levels"],
            ),
            (
                "Board Scanner",
                "human move detection",
                SENSOR,
                ["Polls 96 hall sensors through muxes", "Debounces changing board states", "Matches sensor diffs to legal moves"],
            ),
            (
                "Piece Tracker",
                "piece identity",
                RAIL,
                ["Maps board squares to real pieces", "Preserves capture-slot identity", "Updates after every legal move"],
            ),
            (
                "Board Interpreter",
                "physical path planning",
                COPPER,
                ["Converts engine moves to board paths", "Detects blockers and captures", "Plans safe routes through square centers"],
            ),
            (
                "Gantry Controller",
                "motion execution",
                GCODE,
                ["Serial control for GRBL", "Magnet commands for pickup/release", "Dry-run mode audits every command"],
            ),
        ]

        boxes = VGroup()
        for title, subtitle, color, _ in stages:
            box = RoundedRectangle(
                corner_radius=0.14,
                width=3.12,
                height=0.58,
                color=color,
                fill_color=SURFACE,
                fill_opacity=0.9,
                stroke_width=2.5,
            )
            title_text = self.text(title, font_size=16, weight=BOLD)
            subtitle_text = self.text(subtitle, font_size=10, color=MUTED)
            text = VGroup(title_text, subtitle_text).arrange(DOWN, buff=0.06).move_to(box)
            boxes.add(VGroup(box, text))
        boxes.arrange(DOWN, buff=0.24).shift(LEFT * 4.0 + DOWN * 0.15)

        self.play(FadeIn(heading))
        self.play(LaggedStart(*(FadeIn(box, shift=RIGHT * 0.18) for box in boxes), lag_ratio=0.16))

        current_highlight = None
        current_panel = None
        for index, (title, _, color, bullets) in enumerate(stages):
            highlight = SurroundingRectangle(boxes[index], color=color, buff=0.08, corner_radius=0.16, stroke_width=4)
            panel = self.software_detail_panel(title, bullets, color)

            animations = [Create(highlight)]
            if current_highlight is not None:
                animations.append(FadeOut(current_highlight))
            if current_panel is None:
                animations.append(FadeIn(panel, shift=LEFT * 0.18))
            else:
                animations.append(ReplacementTransform(current_panel, panel))

            self.play(*animations, run_time=0.75)
            self.wait(3.25)
            current_highlight = highlight
            current_panel = panel

        self.play(FadeOut(VGroup(heading, boxes, current_highlight, current_panel)))

    def path_planning_scene(self) -> None:
        heading = self.heading("Problem Solved: Physical Path Planning")
        board = self.make_chessboard(square_size=0.38).shift(LEFT * 2.65 + DOWN * 0.1)
        squares = board

        start = squares[2 * 8 + 2].get_center()
        bend = squares[2 * 8 + 3].get_center()
        mid_leg = squares[3 * 8 + 3].get_center()
        target = squares[4 * 8 + 3].get_center()
        blocker_a_start = bend
        blocker_a_park = squares[1 * 8 + 3].get_center()
        blocker_b_start = mid_leg
        blocker_b_park = squares[3 * 8 + 4].get_center()
        alt_blocker_a_start = squares[3 * 8 + 2].get_center()
        alt_blocker_b_start = squares[4 * 8 + 2].get_center()

        mover = self.board_piece(start, RAIL)
        target_marker = Circle(radius=0.13, color=ENGINE, stroke_width=3).move_to(target).set_z_index(4)
        blocker_a = self.board_piece(blocker_a_start, GCODE)
        blocker_b = self.board_piece(blocker_b_start, PURPLE)
        alt_blocker_a = self.board_piece(alt_blocker_a_start, COPPER)
        alt_blocker_b = self.board_piece(alt_blocker_b_start, COPPER)

        blocked_path = VGroup(
            Line(start, bend, color=GCODE, stroke_width=6),
            Line(bend, target, color=GCODE, stroke_width=6),
        ).set_opacity(0.8).set_z_index(3)
        search_nodes = VGroup(
            *[
                Dot(squares[rank * 8 + file].get_center(), radius=0.035, color=MUTED)
                for rank, file in [(1, 2), (1, 3), (1, 4), (2, 4), (3, 2), (3, 4), (4, 2), (4, 4), (5, 3)]
            ]
        )
        relocate_b = Arrow(blocker_b_start, blocker_b_park, buff=0.16, color=PURPLE, stroke_width=4)
        relocate_a = Arrow(blocker_a_start, blocker_a_park, buff=0.16, color=GCODE, stroke_width=4)
        final_path = VGroup(
            Line(start, bend, color=GCODE, stroke_width=6),
            Line(bend, target, color=GCODE, stroke_width=6),
        ).set_z_index(3)

        legend = VGroup(
            self.text("1. Detect blocked physical route", font_size=18, color=GCODE),
            self.text("2. BFS finds temporary parking", font_size=18, color=PURPLE),
            self.text("3. A* confirms the knight L-path", font_size=18, color=GCODE),
        ).arrange(DOWN, aligned_edge=LEFT, buff=0.16).next_to(board, DOWN, buff=0.25)

        code = self.code_card(
            [
                "path = astar(from, to, occupied)",
                "for blocker in path.blockers:",
                "    park = bfs_nearest_open_square(blocker)",
                "    relocate(blocker, park)  # recursive",
                "return astar(from, to, updated_board)",
            ]
        )

        self.play(FadeIn(heading))
        self.play(FadeIn(board), FadeIn(code, shift=LEFT * 0.15), run_time=0.9)
        self.play(
            GrowFromCenter(mover),
            GrowFromCenter(blocker_a),
            GrowFromCenter(blocker_b),
            GrowFromCenter(alt_blocker_a),
            GrowFromCenter(alt_blocker_b),
            Create(target_marker),
        )
        self.play(Create(blocked_path), FadeIn(legend[0]), run_time=0.9)
        self.play(LaggedStartMap(GrowFromCenter, search_nodes, lag_ratio=0.05), FadeIn(legend[1]), run_time=1.2)
        self.play(Create(relocate_b), blocker_b.animate.move_to(blocker_b_park), run_time=1.1)
        self.play(Create(relocate_a), blocker_a.animate.move_to(blocker_a_park), run_time=1.1)
        self.play(FadeOut(blocked_path), FadeIn(legend[2]), Create(final_path), run_time=1.2)
        self.play(mover.animate.move_to(bend), run_time=0.65)
        self.play(mover.animate.move_to(target), run_time=0.95)
        self.play(blocker_a.animate.move_to(blocker_a_start), run_time=0.8)
        self.play(blocker_b.animate.move_to(blocker_b_start), run_time=0.8)
        self.wait(1.9)
        self.play(
            FadeOut(
                VGroup(
                    heading,
                    board,
                    mover,
                    target_marker,
                    blocker_a,
                    blocker_b,
                    alt_blocker_a,
                    alt_blocker_b,
                    search_nodes,
                    relocate_a,
                    relocate_b,
                    final_path,
                    legend,
                    code,
                )
            )
        )

    def capture_identity_scene(self) -> None:
        heading = self.heading("Problem Solved: Capture Identity")
        board = self.make_board_with_storage().scale(0.9).shift(LEFT * 2.25 + DOWN * 0.15)
        board_squares = board[0]
        left_storage = board[1]
        right_storage = board[2]

        mover = self.board_piece(board_squares[3 * 8 + 3].get_center(), SENSOR)
        captured_piece = self.board_piece(board_squares[4 * 8 + 4].get_center(), GCODE)
        same_type_a = self.board_piece(board_squares[0 * 8 + 0].get_center(), GCODE).scale(0.9)
        same_type_b = self.board_piece(board_squares[7 * 8 + 7].get_center(), GCODE).scale(0.9)
        capture_arrow = Arrow(
            mover.get_center(),
            captured_piece.get_center(),
            buff=0.16,
            color=COPPER,
            stroke_width=5,
        )

        left_labels = self.storage_labels(left_storage, [f"P{i}" for i in range(1, 9)], mirror=True)
        right_labels = self.storage_labels(right_storage, ["R1", "N1", "B1", "Q", "K", "B2", "N2", "R2"])
        slot_r1 = right_storage[0]
        slot_r2 = right_storage[14]
        slot_highlight_a = SurroundingRectangle(slot_r1, color=GCODE, buff=0.04, corner_radius=0.08, stroke_width=3)
        slot_highlight_b = SurroundingRectangle(slot_r2, color=GCODE, buff=0.04, corner_radius=0.08, stroke_width=3)
        route_to_slot = Arrow(
            captured_piece.get_center(),
            slot_r1.get_center(),
            buff=0.18,
            color=ENGINE,
            stroke_width=5,
        )

        legend = VGroup(
            self.text("Hall sensors read occupancy, not piece identity", font_size=18, color=GCODE),
            self.text("Labeled slots let captures map to a specific physical piece", font_size=18, color=ENGINE),
            self.text("Future improvement: RFID-tagged pieces", font_size=18, color=RAIL),
        ).arrange(DOWN, aligned_edge=LEFT, buff=0.16).next_to(board, DOWN, buff=0.28)

        problem = self.code_card(
            [
                "capture detected -> one piece removed",
                "sensor data: occupied / empty only",
                "ambiguous: was it R1 or R2?",
                "without IDs, restore paths become vague",
            ],
            title="identity problem",
        )
        problem.shift(RIGHT * 1.55)
        solution = self.code_card(
            [
                "piece_tracker[square] = slot_id",
                "capture on e5 -> remove tracked piece",
                "send physical piece to labeled slot",
                "future: replace slot IDs with RFID reads",
            ],
            title="identity solution",
        )
        solution.shift(RIGHT * 1.55 + DOWN * 0.15)

        self.play(FadeIn(heading))
        self.play(FadeIn(board), FadeIn(problem, shift=LEFT * 0.15), run_time=0.9)
        self.play(
            GrowFromCenter(mover),
            GrowFromCenter(captured_piece),
            GrowFromCenter(same_type_a),
            GrowFromCenter(same_type_b),
            run_time=0.8,
        )
        self.play(FadeIn(left_labels), FadeIn(right_labels), run_time=0.8)
        self.play(Create(capture_arrow), FadeIn(legend[0]), run_time=0.9)
        self.play(Create(slot_highlight_a), Create(slot_highlight_b), run_time=0.8)
        self.play(
            FadeOut(problem),
            FadeIn(solution, shift=LEFT * 0.15),
            Create(route_to_slot),
            FadeIn(legend[1]),
            run_time=1.0,
        )
        self.play(FadeOut(captured_piece), run_time=0.45)
        self.play(route_to_slot.animate.set_opacity(0.35), run_time=0.3)
        self.play(FadeIn(legend[2]), run_time=0.8)
        self.wait(2.0)
        self.play(
            FadeOut(
                VGroup(
                    heading,
                    board,
                    mover,
                    captured_piece,
                    same_type_a,
                    same_type_b,
                    capture_arrow,
                    left_labels,
                    right_labels,
                    slot_highlight_a,
                    slot_highlight_b,
                    route_to_slot,
                    legend,
                    solution,
                )
            )
        )

    def closing_scene(self) -> None:
        board = self.make_board_with_storage().scale(0.95).shift(DOWN * 0.35)
        board_squares = board[0]
        gantry_start = board_squares[0 * 8 + 0].get_center() + LEFT * 0.15
        gantry = self.make_gantry().scale(0.95).shift(DOWN * 0.35)
        gantry[2].move_to(gantry_start)
        gantry[1].put_start_and_end_on(
            np.array([gantry[0][0].get_x(), gantry_start[1], 0]),
            np.array([gantry[0][1].get_x(), gantry_start[1], 0]),
        )
        statement = self.text(
            "Kirin lets you play physical chess with no opponent.",
            font_size=25,
            weight=BOLD,
        )
        statement.scale_to_fit_width(config.frame_width - 1.8)
        statement.to_edge(UP, buff=0.65)
        subline = self.text(
            "The system acts as your opponent.",
            font_size=20,
            color=MUTED,
        ).next_to(statement, DOWN, buff=0.18)

        piece_end = board_squares[3 * 8 + 4].get_center()
        opponent_piece_start = board_squares[6 * 8 + 4].get_center()
        opponent_piece_end = board_squares[4 * 8 + 4].get_center()

        player_piece = self.board_piece(piece_end, SENSOR)
        opponent_piece = self.board_piece(opponent_piece_start, GCODE)
        carriage_and_opponent = VGroup(gantry[2], opponent_piece)
        y_axis_with_opponent = VGroup(gantry[1], gantry[2], opponent_piece)

        motion_trace = VGroup(
            Line(gantry_start, np.array([opponent_piece_start[0], gantry_start[1], 0]), color=MUTED, stroke_width=4),
            Line(np.array([opponent_piece_start[0], gantry_start[1], 0]), opponent_piece_start, color=MUTED, stroke_width=4),
            Line(opponent_piece_start, opponent_piece_end, color=GCODE, stroke_width=5),
        ).set_opacity(0.75)

        self.play(FadeIn(statement, shift=DOWN), FadeIn(subline, shift=DOWN))
        self.play(Create(board), Create(gantry), GrowFromCenter(player_piece), GrowFromCenter(opponent_piece), run_time=1.8)
        self.play(gantry[2].animate.shift(RIGHT * (opponent_piece_start[0] - gantry_start[0])), Create(motion_trace[0]), run_time=0.8)
        self.play(VGroup(gantry[1], gantry[2]).animate.shift(UP * (opponent_piece_start[1] - gantry_start[1])), Create(motion_trace[1]), run_time=0.8)
        self.play(
            gantry[2][1].animate.set_fill(GCODE).set_color(GCODE),
            opponent_piece.animate.scale(0.85),
            run_time=0.35,
        )
        self.play(y_axis_with_opponent.animate.shift(UP * (opponent_piece_end[1] - opponent_piece_start[1])), Create(motion_trace[2]), run_time=1.5)
        self.play(
            gantry[2][1].animate.set_fill(INK).set_color(INK),
            opponent_piece.animate.scale(1 / 0.85),
            run_time=0.35,
        )
        self.wait(3.5)
        self.play(FadeOut(VGroup(statement, subline, board, gantry, player_piece, opponent_piece, motion_trace)))

    def text(self, value: str, font_size: int, color: str = INK, weight: str = NORMAL) -> Text:
        return Text(value, font=FONT, font_size=font_size, color=color, weight=weight)

    def heading(self, text: str) -> Text:
        return self.text(text, font_size=42, weight=BOLD).to_edge(UP, buff=0.45)

    def make_chessboard(self, square_size: float = 0.45) -> VGroup:
        squares = VGroup()
        for rank in range(8):
            for file in range(8):
                color = BOARD_LIGHT if (rank + file) % 2 == 0 else BOARD_DARK
                square = Square(side_length=square_size, fill_color=color, fill_opacity=1, stroke_width=0)
                square.move_to(np.array([file * square_size, rank * square_size, 0]))
                squares.add(square)
        squares.move_to(ORIGIN)
        return squares

    def make_board_with_storage(self) -> VGroup:
        square_size = 0.43
        board = self.make_chessboard(square_size=square_size)
        left_storage = self.make_storage_zone(square_size=square_size, mirror=True)
        right_storage = self.make_storage_zone(square_size=square_size)
        left_storage.next_to(board, LEFT, buff=0.18)
        right_storage.next_to(board, RIGHT, buff=0.18)

        return VGroup(board, left_storage, right_storage).move_to(ORIGIN)

    def make_storage_zone(self, square_size: float, mirror: bool = False) -> VGroup:
        zone = VGroup()
        for rank in range(8):
            for file in range(2):
                is_inner_file = file == (1 if mirror else 0)
                color = SURFACE if is_inner_file else BOARD_LIGHT
                square = Square(
                    side_length=square_size,
                    fill_color=color,
                    fill_opacity=1,
                    stroke_color=PURPLE,
                    stroke_width=1.5,
                )
                square.move_to(np.array([file * square_size, rank * square_size, 0]))
                zone.add(square)
        zone.move_to(ORIGIN)
        return zone

    def storage_labels(self, zone: VGroup, labels: list[str], mirror: bool = False) -> VGroup:
        file_index = 1 if mirror else 0
        text_labels = VGroup()
        for rank, label in enumerate(labels):
            square = zone[rank * 2 + file_index]
            text_labels.add(self.text(label, font_size=11, color=INK, weight=BOLD).move_to(square))
        return text_labels.set_z_index(4)

    def make_gantry(self, carriage_start: np.ndarray | None = None) -> VGroup:
        if carriage_start is None:
            carriage_start = np.array([-0.7, 1.15, 0])

        left_rail = Line(UP * 2.15, DOWN * 2.15, color=RAIL, stroke_width=9).shift(LEFT * 3.15)
        right_rail = left_rail.copy().shift(RIGHT * 6.3)
        side_rails = VGroup(left_rail, right_rail)

        crossbar_y = carriage_start[1]
        crossbar = Line(
            np.array([left_rail.get_x(), crossbar_y, 0]),
            np.array([right_rail.get_x(), crossbar_y, 0]),
            color=RAIL,
            stroke_width=10,
        )
        carriage = VGroup(
            RoundedRectangle(
                corner_radius=0.08,
                width=0.48,
                height=0.48,
                color=GCODE,
                fill_color=GCODE,
                fill_opacity=1,
                stroke_width=2,
            ),
            Circle(radius=0.16, color=INK, fill_color=INK, fill_opacity=1, stroke_width=2),
        ).move_to(carriage_start)
        carriage[0].set_z_index(2)
        carriage[1].move_to(carriage[0].get_center()).set_z_index(3)

        rail_caps = VGroup()
        for rail in side_rails:
            rail_caps.add(Dot(rail.get_start(), radius=0.06, color=PURPLE))
            rail_caps.add(Dot(rail.get_end(), radius=0.06, color=PURPLE))

        return VGroup(side_rails, crossbar, carriage, rail_caps)

    def callout(self, text: str, target: Mobject, direction: np.ndarray) -> VGroup:
        label = self.text(text, font_size=18)
        label.next_to(target, direction, buff=0.15)
        return label

    def mux_box(self, title: str, subtitle: str) -> VGroup:
        box = RoundedRectangle(
            corner_radius=0.08,
            width=1.34,
            height=0.54,
            color=RAIL,
            fill_color=SURFACE,
            fill_opacity=0.92,
            stroke_width=2,
        )
        text = VGroup(
            self.text(title, font_size=16, weight=BOLD),
            self.text(subtitle, font_size=9, color=MUTED),
        ).arrange(DOWN, buff=0.04).move_to(box)
        return VGroup(box, text)

    def ui_button(self, label: str, color: str) -> VGroup:
        button = Circle(radius=0.34, color=color, fill_color=SURFACE, fill_opacity=1, stroke_width=3)
        text = self.text(label, font_size=16, color=INK, weight=BOLD).next_to(button, RIGHT, buff=0.22)
        return VGroup(button, text)

    def software_detail_panel(self, title: str, bullets: list[str], color: str) -> VGroup:
        panel = RoundedRectangle(
            corner_radius=0.18,
            width=5.9,
            height=3.25,
            color=color,
            fill_color=SURFACE,
            fill_opacity=0.94,
            stroke_width=3,
        ).shift(RIGHT * 1.85 + DOWN * 0.08)
        title_text = self.text(title, font_size=23, color=color, weight=BOLD)
        bullet_lines = VGroup(
            *[self.text(f"- {bullet}", font_size=14) for bullet in bullets]
        ).arrange(DOWN, aligned_edge=LEFT, buff=0.16)
        content = VGroup(title_text, bullet_lines).arrange(DOWN, aligned_edge=LEFT, buff=0.26)
        content.next_to(panel.get_left(), RIGHT, buff=0.32)
        content.set_y(panel.get_y())
        return VGroup(panel, content)

    def board_piece(self, position: np.ndarray, color: str) -> VGroup:
        base = Circle(radius=0.13, color=INK, fill_color=color, fill_opacity=1, stroke_width=2)
        return VGroup(base).move_to(position).set_z_index(5)

    def code_card(self, lines: list[str], title: str = "recursive relocation") -> VGroup:
        panel = RoundedRectangle(
            corner_radius=0.14,
            width=5.3,
            height=2.4,
            color=RAIL,
            fill_color=SURFACE,
            fill_opacity=0.95,
            stroke_width=2,
        ).shift(RIGHT * 2.15 + UP * 0.35)
        title = self.text(title, font_size=19, color=RAIL, weight=BOLD)
        code_lines = VGroup(*[self.text(line, font_size=12, color=INK) for line in lines]).arrange(
            DOWN, aligned_edge=LEFT, buff=0.09
        )
        content = VGroup(title, code_lines).arrange(DOWN, aligned_edge=LEFT, buff=0.18)
        content.next_to(panel.get_left(), RIGHT, buff=0.28)
        content.set_y(panel.get_y())
        return VGroup(panel, content)

    def make_move_trace(self) -> VGroup:
        board = self.make_chessboard(square_size=0.28)
        start = board[1 * 8 + 1].get_center()
        bend = board[4 * 8 + 1].get_center()
        end = board[4 * 8 + 5].get_center()
        path = VMobject(color=GCODE, stroke_width=6)
        path.set_points_as_corners([start, bend, end])
        piece = Circle(radius=0.12, color=INK, fill_color=INK, fill_opacity=1).move_to(start)
        ghost = Circle(radius=0.12, color=GCODE, stroke_width=3).move_to(end)
        return VGroup(board, path, piece, ghost)
