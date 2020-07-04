rm ./lib/*
echo "==v100nptl=="
make clean
make
echo "==v200=="
make V200=Y clean
make V200=Y
echo "==v300=="
make V300=Y clean 
make V300=Y
echo "==v500=="
make V500=Y clean 
make V500=Y
echo "==v510=="
make V510=Y clean
make V510=Y
echo "==x86=="
make X86=Y clean 
make X86=Y
echo "==v100=="
make V100=Y clean 
make V100=Y
echo "==Varago=="
make Varago=Y clean
make Varago=Y

echo "==test=="
cd ./test
./buildtest.sh
echo "==end=="
