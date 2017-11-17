select e.strategy, e.input, s.name, s.variable, s.value
from Singletons s
join Experiments e
on e.run = s.run
where s.variable="ReceivePackets-total"
order by e.input