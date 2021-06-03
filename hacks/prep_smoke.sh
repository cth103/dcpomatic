DCP=$HOME/DCP/Examples/xm
KEY=$HOME/dcpomatic-infrastructure/keys/test-vm-id_rsa
targets="centos-7-64 centos-8-64 debian-9-64 debian-10-64 ubuntu-16.04-32 ubuntu-16.04-64 ubuntu-18.04-64 ubuntu-20.04-64 ubuntu-20.10-64 ubuntu-21.04-64 fedora-32-64 fedora-33-64 fedora-34-64"

opts="-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -i $KEY"

for t in $targets; do
	host=$(echo $t | sed "s/\./-/g")
	ping -c 1 $host.local
	if [ "$?" == "0" ]; then
		scp $opts -r $DCP $host.local:
		scp $opts -r $t/* $host.local:
	fi
done
