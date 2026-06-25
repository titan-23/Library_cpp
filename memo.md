beam_search/
です

beam_search.cppとbeam_search_state.cpp

beam_search_turn.cppとbeam_search_state_turn.cpp

がそれぞれ対応していると思います。
それぞれ、
- ロジックとして正しいかどうか
- コメントは適切か(ビームサーチ本体は隠蔽されて、state側のみをみてユーザーは実装します)
- beam_search_state.cpp、beam_search_state_turn.cppで、同一なところは同一になっているか
等について精査してください。
