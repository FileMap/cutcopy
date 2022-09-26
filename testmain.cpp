int main(int argc, char *argv[])
{
    auto v = ReadFilePaths();
    pv(v);

    int cutOp = isCut();


    std::string f1 = "F:/filemap/test/clipboardWinTestCpp/test1";
    std::string f2 = "F:/filemap/test/clipboardWinTestCpp/test2";
    std::vector<std::string> vwrite;
    vwrite.push_back(f1);
    vwrite.push_back(f2);
    changeDropEffect(DROPEFFECT_MOVE);
    WriteFilePaths(vwrite);

    return 0;
}